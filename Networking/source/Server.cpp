#include "Server.hpp"

#include <iostream>

namespace ignite {

static Server *s_Instance = nullptr;

Server::Server(int port)
    : m_Port(port)
{
}

Server::~Server()
{
    if (m_NetworkThread.joinable())
        m_NetworkThread.join();
}

void Server::Start()
{
    if (m_Running)
        return;

    m_NetworkThread = std::thread([this]() { NetworkThreadFunc(); });
}

void Server::Stop()
{
    m_Running = false;
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
    for (const auto &[client_id, client_info] : m_ConnectedClients)
    {
        if (client_id != exclude_client_id)
            SendBufferToClient(client_id, buffer, reliable);
    }
}

void Server::KickClient(ClientID client_id)
{
    m_Interface->CloseConnection(client_id, 0, "Kicked by host", false);
}

void Server::NetworkThreadFunc()
{
    s_Instance = this;
    m_Running = true;

    SteamDatagramErrMsg error_message;
    GameNetworkingSockets_Init(nullptr, error_message);

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
    for (const auto &[client_id, client_info] : m_ConnectedClients)
    {
        m_Interface->CloseConnection(client_id, 0, "Server shutdown", true);
    }

    m_ConnectedClients.clear();

    m_Interface->CloseListenSocket(m_ListenSocket);
    m_ListenSocket = k_HSteamListenSocket_Invalid;

    m_Interface->DestroyPollGroup(m_PollGroup);
    m_PollGroup = k_HSteamNetPollGroup_Invalid;
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
            auto it_client = m_ConnectedClients.find(status->m_hConn);

            m_ClientDisconnectedCallback(it_client->second);
            m_ConnectedClients.erase(it_client);
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
        auto &client = m_ConnectedClients[status->m_hConn];
        client.ID = (ClientID)status->m_hConn;
        client.ConnectionDesc = connection_info.m_szConnectionDescription;

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

        auto it_client = m_ConnectedClients.find(incoming_message->m_conn);
        if (it_client == m_ConnectedClients.end())
        {
            std::cout << "Error: Received data from unregistered client\n";
            continue;
        }

        if (incoming_message->m_cbSize)
        {
            m_DataReceivedCallback(it_client->second, Buffer(incoming_message->m_pData, incoming_message->m_cbSize));
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