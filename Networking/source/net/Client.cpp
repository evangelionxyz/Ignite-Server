#include "Client.hpp"
#include "GnsInit.hpp"
#include <iostream>

namespace ignite::net {

    static Client *s_Instance = nullptr;

    Client::~Client()
    {
        if (m_NetworkThread.joinable())
            m_NetworkThread.join();
    }

    void Client::ConnectToServer(const std::string &server_address)
    {
        if (m_Running)
            return;

        if (m_NetworkThread.joinable())
            m_NetworkThread.join();

        m_ServerAddress = server_address;
        m_NetworkThread = std::thread([this]() { NetworkThreadFunc(); });
    }

    void Client::Disconnect()
    {
        m_Running = false;
        
        if (m_NetworkThread.joinable())
            m_NetworkThread.join();
    }

    void Client::PushEvent(NetEvent &&event)
    {
        std::lock_guard<std::mutex> lock(m_EventMutex);
        m_Events.push_back(std::move(event));
    }

    size_t Client::PumpEvents()
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
            case NetEvent::Type::Connected:
                if (m_ServerConnectedCallback) m_ServerConnectedCallback();
                break;
            case NetEvent::Type::Disconnected:
                if (m_ServerDisconnectedCallback) m_ServerDisconnectedCallback();
                break;
            case NetEvent::Type::DataReceived:
                if (m_DataReceivedCallback)
                    m_DataReceivedCallback(Buffer(event.Data.data(), event.Data.size()));
                break;
            }
        }
        return events.size();
    }

    void Client::SetDataReceivedCallback(const DataReceivedCallback &func)
    {
        m_DataReceivedCallback = func;
    }

    void Client::SetServerConnectedCallback(const ServerConnectedCallback &func)
    {
        m_ServerConnectedCallback = func;
    }

    void Client::SetServerDisconnectedCallback(const ServerDisconnectedCallback &func)
    {
        m_ServerDisconnectedCallback = func;
    }

    void Client::SendBuffer(Buffer buffer, bool reliable /*= true*/)
    {
        if (!m_Interface || m_ConnectionStatus != ConnectionStatus::Connected)
            return;

        EResult result = m_Interface->SendMessageToConnection(m_Connection, buffer.Data, (uint32_t)buffer.Size,
            reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable, nullptr);
    }

    void Client::SendString(const std::string &message, bool reliable /*= true*/)
    {
        SendBuffer(Buffer(message.data(), message.size()), reliable);
    }

    void Client::NetworkThreadFunc()
    {
        s_Instance = this;

        m_ConnectionStatus = ConnectionStatus::Connecting;

        SteamDatagramErrMsg error_message;
        if (!internal::InitGNS(error_message))
        {
            m_ConnectionDebugMessage = "Could not initialize GameNetworkingSockets";
            m_ConnectionStatus = ConnectionStatus::FailedToConnect;
            return;
        }

        m_Interface = SteamNetworkingSockets();

        // start connecting
        SteamNetworkingIPAddr address;
        if (!address.ParseString(m_ServerAddress.c_str()))
        {
            OnFatalError("Invalid IP address - could not parse " + m_ServerAddress);
            m_ConnectionDebugMessage = "Invalid IP address";
            m_ConnectionStatus = ConnectionStatus::FailedToConnect;
            return;
        }

        // Tag the connection with this instance so several clients can live in one process.
        SteamNetworkingConfigValue_t options[2];
        options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)Client::ConnectionstatusChangedCallback);
        options[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, (int64)(intptr_t)this);
        m_Connection = m_Interface->ConnectByIPAddress(address, 2, options);
        if (m_Connection == k_HSteamNetConnection_Invalid)
        {
            m_ConnectionDebugMessage = "Failed to create connection";
            m_ConnectionStatus = ConnectionStatus::FailedToConnect;
            return;
        }

        m_Running = true;
        while (m_Running)
        {
            PollIncomingMessages();
            PollConnectionStateChanges();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        m_Interface->CloseConnection(m_Connection, 0, nullptr, false);
        m_ConnectionStatus = ConnectionStatus::Disconnected;
        internal::KillGNS();
    }

    void Client::Shutdown()
    {
        m_Running = false;
    }

    void Client::ConnectionstatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info)
    {
        Client *client = reinterpret_cast<Client *>((intptr_t)info->m_info.m_nUserData);
        if (!client)
            client = s_Instance;
        if (client)
            client->OnConnectionStatusChanged(info);
    }

    void Client::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status)
    {
        switch (status->m_info.m_eState)
        {
        case k_ESteamNetworkingConnectionState_None:
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            m_Running = false;
            const bool wasConnected = status->m_eOldState == k_ESteamNetworkingConnectionState_Connected;
            m_ConnectionStatus = ConnectionStatus::FailedToConnect;
            m_ConnectionDebugMessage = status->m_info.m_szEndDebug;

            if (status->m_eOldState == k_ESteamNetworkingConnectionState_Connecting)
            {
                std::cout << "Could not connect to remove host. " << status->m_info.m_szEndDebug << std::endl;
            }
            else if (status->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
            {
                std::cout << "Lost connection with remote host. " << status->m_info.m_szEndDebug << std::endl;
            }
            else
            {
                std::cout << "Disconnected from host. " << status->m_info.m_szEndDebug << std::endl;
            }

            m_Interface->CloseConnection(status->m_hConn, 0, nullptr, false);
            m_Connection = k_HSteamNetConnection_Invalid;
            m_ConnectionStatus = ConnectionStatus::Disconnected;

            if (wasConnected)
            {
                if (m_UseEventQueue)
                    PushEvent({ NetEvent::Type::Disconnected, {} });
                else if (m_ServerDisconnectedCallback)
                    m_ServerDisconnectedCallback();
            }
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            m_ConnectionStatus = ConnectionStatus::Connected;
            if (m_UseEventQueue)
                PushEvent({ NetEvent::Type::Connected, {} });
            else if (m_ServerConnectedCallback)
                m_ServerConnectedCallback();
            break;

        default:
            break;
        }
    }

    void Client::PollIncomingMessages()
    {
        while (m_Running)
        {
            ISteamNetworkingMessage *incoming_message = nullptr;
            int message_count = m_Interface->ReceiveMessagesOnConnection(m_Connection, &incoming_message, 1);
            if (message_count == 0)
                break;

            if (message_count < 0)
            {
                m_Running = false;
                break;
            }

            if (m_UseEventQueue)
            {
                const uint8_t *bytes = static_cast<const uint8_t *>(incoming_message->m_pData);
                PushEvent({ NetEvent::Type::DataReceived, std::vector<uint8_t>(bytes, bytes + incoming_message->m_cbSize) });
            }
            else if (m_DataReceivedCallback)
            {
                m_DataReceivedCallback(Buffer(incoming_message->m_pData, incoming_message->m_cbSize));
            }

            incoming_message->Release();
        }
    }

    void Client::PollConnectionStateChanges()
    {
        m_Interface->RunCallbacks();
    }

    void Client::OnFatalError(const std::string &message)
    {
        std::cout << message << std::endl;
        m_Running = false;
    }

    bool Client::GetRealTimeStatus(SteamNetConnectionRealTimeStatus_t *status) const
    {
        if (m_Interface && m_Connection != k_HSteamNetConnection_Invalid && status)
        {
            return m_Interface->GetConnectionRealTimeStatus(m_Connection, status, 0, nullptr) == k_EResultOK;
        }
        return false;
    }
}
