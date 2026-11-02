#include "Server.hpp"
#include "GnsInit.hpp"

#include <iostream>

namespace ignite::net
{
    static Server *s_Instance = nullptr;

    Server::Server(int port)
        : m_Port(port)
    {
    }

    Server::~Server()
    {
        m_Running = false;
        if (m_NetworkThread.joinable())
            m_NetworkThread.join();
    }

    void Server::Start()
    {
        if (m_Running)
            return;

        if (m_NetworkThread.joinable())
            m_NetworkThread.join();

        m_Running = true;
        m_NetworkThread = std::thread([this]() { NetworkThreadFunc(); });
    }

    void Server::Stop()
    {
        m_Running = false;
        if (m_NetworkThread.joinable() && m_NetworkThread.get_id() != std::this_thread::get_id())
            m_NetworkThread.join();
    }

    void Server::PushEvent(NetEvent &&event)
    {
        std::lock_guard<std::mutex> lock(m_EventMutex);
        m_Events.push_back(std::move(event));
    }

    size_t Server::PumpEvents()
    {
        std::vector<NetEvent> events;
        {
            std::lock_guard<std::mutex> lock(m_EventMutex);
            events.swap(m_Events);
        }

        for (const NetEvent &event : events)
        {
            switch (event.EventType)
            {
            case NetEvent::Type::ClientConnected:
                if (m_ClientConnectedCallback) m_ClientConnectedCallback(event.Client);
                break;
            case NetEvent::Type::ClientDisconnected:
                if (m_ClientDisconnectedCallback) m_ClientDisconnectedCallback(event.Client);
                break;
            case NetEvent::Type::DataReceived:
                if (m_DataReceivedCallback)
                    m_DataReceivedCallback(event.Client, Buffer(event.Data.data(), event.Data.size()));
                break;
            }
        }
        return events.size();
    }

    void Server::SetDataReceivedCallback(const DataReceivedCallback &func)
    {
        m_DataReceivedCallback = func;
    }

    void Server::SetClientConnectedCallback(const ClientConnectedCallback &func)
    {
        m_ClientConnectedCallback = func;
    }

    void Server::SetClientDisconnectedCallback(const ClientDisconnectedCallback &func)
    {
        m_ClientDisconnectedCallback = func;
    }

    void Server::SendBufferToClient(ClientID client_id, Buffer buffer, bool reliable /*= true*/)
    {
        if (!m_Interface)
            return;

        m_Interface->SendMessageToConnection((HSteamNetConnection)client_id, buffer.Data, (uint32_t)buffer.Size, 
            reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable, nullptr);
    }

    void Server::SendStringToClient(ClientID client_id, const std::string &message, bool reliable)
    {
        SendBufferToClient(client_id, Buffer(message.data(), message.size()), reliable);
    }

    void Server::SendStringToClient(const std::string &message, ClientID exclude_client_id /*= 0*/, bool reliable)
    {
        SendBufferToAllClients(Buffer(message.data(), message.size()), exclude_client_id, reliable);
    }

    void Server::SendBufferToAllClients(Buffer buffer, ClientID exclude_client_id /*= 0*/, bool reliable /*= true*/)
    {
        std::lock_guard<std::mutex> lock(m_ClientsMutex);
        for (const auto &[client_id, client_info] : m_ConnectedClients)
        {
            if (client_id != exclude_client_id)
                SendBufferToClient(client_id, buffer, reliable);
        }
    }

    void Server::KickClient(ClientID client_id)
    {
        if (!m_Interface)
            return;

        // linger so that a queued reliable message (e.g. a reject reason) is flushed first
        m_Interface->CloseConnection(client_id, 0, "Kicked by host", true);

        // A locally closed connection does not raise a status-changed callback,
        // so remove the client and notify here.
        ClientInfo info{};
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(m_ClientsMutex);
            auto it = m_ConnectedClients.find((HSteamNetConnection)client_id);
            if (it != m_ConnectedClients.end())
            {
                info = it->second;
                m_ConnectedClients.erase(it);
                found = true;
            }
        }

        if (found)
        {
            if (m_UseEventQueue)
                PushEvent({ NetEvent::Type::ClientDisconnected, info, {} });
            else if (m_ClientDisconnectedCallback)
                m_ClientDisconnectedCallback(info);
        }
    }

    void Server::NetworkThreadFunc()
    {
        s_Instance = this;

        SteamDatagramErrMsg error_message;
        internal::InitGNS(error_message);

        m_Interface = SteamNetworkingSockets();

        // start listening
        SteamNetworkingIPAddr server_local_address;
        server_local_address.Clear();
        server_local_address.m_port = m_Port;

        SteamNetworkingConfigValue_t options;
        options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)Server::ConnectionStatusChangedCallback);

        // try to start listen socket on port
        m_ListenSocket = m_Interface->CreateListenSocketIP(server_local_address, 1, &options);

        if (m_ListenSocket == k_HSteamListenSocket_Invalid)
        {
            OnFatalError("Fatal error: failed to listen on port: " + std::to_string(m_Port));
            return;
        }


        m_PollGroup = m_Interface->CreatePollGroup();
        if (m_PollGroup == k_HSteamNetPollGroup_Invalid)
        {
            OnFatalError("Fatal error: failed to listen on port: " + std::to_string(m_Port));
            return;
        }

        std::cout << "Server listening on port " << m_Port << std::endl;

        while (m_Running)
        {
            PollIncomingMessages();
            PollConnectionStateChanges();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // close all the connections
        {
            std::lock_guard<std::mutex> lock(m_ClientsMutex);
            for (const auto &[client_id, client_info] : m_ConnectedClients)
            {
                m_Interface->CloseConnection(client_id, 0, "Server shutdown", true);
            }

            m_ConnectedClients.clear();
        }

        m_Interface->CloseListenSocket(m_ListenSocket);
        m_ListenSocket = k_HSteamListenSocket_Invalid;

        m_Interface->DestroyPollGroup(m_PollGroup);
        m_PollGroup = k_HSteamNetPollGroup_Invalid;
        internal::KillGNS();
    }

    void Server::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info)
    {
        s_Instance->OnConnectionStatusChanged(info);
    }

    void Server::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status)
    {
        switch (status->m_info.m_eState)
        {
        case k_ESteamNetworkingConnectionState_None:
            break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            if (status->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
            {
                ClientInfo info{};
                bool found = false;
                {
                    std::lock_guard<std::mutex> lock(m_ClientsMutex);
                    auto it_client = m_ConnectedClients.find(status->m_hConn);
                    if (it_client != m_ConnectedClients.end())
                    {
                        info = it_client->second;
                        m_ConnectedClients.erase(it_client);
                        found = true;
                    }
                }

                if (found)
                {
                    if (m_UseEventQueue)
                        PushEvent({ NetEvent::Type::ClientDisconnected, info, {} });
                    else if (m_ClientDisconnectedCallback)
                        m_ClientDisconnectedCallback(info);
                }
            }

            m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
            break;

        case k_ESteamNetworkingConnectionState_Connecting:
        {
            if (m_Interface->AcceptConnection(status->m_hConn) != k_EResultOK)
            {
                m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
                std::cout << "Couldn't accept connection (it was already closed?" << std::endl;
                break;
            }

            if (!m_Interface->SetConnectionPollGroup(status->m_hConn, m_PollGroup))
            {
                m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
                std::cout << "Failed to set poll group" << std::endl;
                break;
            }

            // retrieve connection info
            SteamNetConnectionInfo_t connection_info;
            m_Interface->GetConnectionInfo(status->m_hConn, &connection_info);

            // register connection client
            ClientInfo client{};
            client.ID = (ClientID)status->m_hConn;
            client.ConnectionDesc = connection_info.m_szConnectionDescription;
            {
                std::lock_guard<std::mutex> lock(m_ClientsMutex);
                m_ConnectedClients[status->m_hConn] = client;
            }

            if (m_UseEventQueue)
                PushEvent({ NetEvent::Type::ClientConnected, client, {} });
            else if (m_ClientConnectedCallback)
                m_ClientConnectedCallback(client);

            break;
        }
        case k_ESteamNetworkingConnectionState_Connected:
            break;
        }
    }

    void Server::PollIncomingMessages()
    {
        m_Interface->RunCallbacks();
    }

    void Server::SetClientNick(HSteamNetConnection h_connections, const char *nick)
    {
        m_Interface->SetConnectionName(h_connections, nick);
    }

    void Server::PollConnectionStateChanges()
    {
        while (m_Running)
        {
            ISteamNetworkingMessage *incoming_message = nullptr;
            int message_count = m_Interface->ReceiveMessagesOnPollGroup(m_PollGroup, &incoming_message, 1);
            if (message_count == 0)
                break;

            if (message_count < 0)
            {
                m_Running = false;
                return;
            }

            ClientInfo info{};
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(m_ClientsMutex);
                auto it_client = m_ConnectedClients.find(incoming_message->m_conn);
                if (it_client != m_ConnectedClients.end())
                {
                    info = it_client->second;
                    found = true;
                }
            }

            if (!found)
            {
                std::cout << "Error: Received data from unregistered client\n";
                incoming_message->Release();
                continue;
            }

            if (incoming_message->m_cbSize)
            {
                if (m_UseEventQueue)
                {
                    const uint8_t *bytes = static_cast<const uint8_t *>(incoming_message->m_pData);
                    PushEvent({ NetEvent::Type::DataReceived, info, std::vector<uint8_t>(bytes, bytes + incoming_message->m_cbSize) });
                }
                else if (m_DataReceivedCallback)
                {
                    m_DataReceivedCallback(info, Buffer(incoming_message->m_pData, incoming_message->m_cbSize));
                }
            }

            incoming_message->Release();
        }
    }

    void Server::OnFatalError(const std::string &message)
    {
        std::cout << message << std::endl;
        m_Running = false;
    }
}