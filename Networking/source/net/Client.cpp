#include "Client.hpp"
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
        if (!GameNetworkingSockets_Init(nullptr, error_message))
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

        SteamNetworkingConfigValue_t options;
        options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)Client::ConnectionstatusChangedCallback);
        m_Connection = m_Interface->ConnectByIPAddress(address, 1, &options);
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
        GameNetworkingSockets_Kill();
    }

    void Client::Shutdown()
    {
        m_Running = false;
    }

    void Client::ConnectionstatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info)
    {
        s_Instance->OnConnectionStatusChanged(info);
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
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            m_ConnectionStatus = ConnectionStatus::Connected;
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

            m_DataReceivedCallback(Buffer(incoming_message->m_pData, incoming_message->m_cbSize));

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
}