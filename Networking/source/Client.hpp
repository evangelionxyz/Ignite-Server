#pragma once

#include "Buffer.hpp"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#   include <steam/steam_api.h>
#endif

#include <string>
#include <thread>
#include <functional>

namespace ignite {
class Client
{
public:
    enum class ConnectionStatus
    {
        Disconnected = 0, Connected, Connecting, FailedToConnect
    };
    using DataReceivedCallback = std::function<void(const Buffer)>;
    using ServerConnectedCallback = std::function<void()>;
    using ServerDisconnectedCallback = std::function<void()>;

    Client() = default;
    ~Client();

    void ConnectToServer(const std::string &server_address);
    void Disconnect();

    void SetDataReceivedCallback(const DataReceivedCallback &func);
    void SetServerConnectedCallback(const ServerConnectedCallback &func);
    void SetServerDisconnectedCallback(const ServerDisconnectedCallback &func);

    void SendBuffer(Buffer buffer, bool reliable = true);
    void SendString(const std::string &string, bool reliable = true);

    template<typename T>
    void SendData(const T &data, bool reliable = true)
    {
        SendBuffer(Buffer(&data, sizeof(T)), reliable);
    }

    bool IsRunning() const { return m_Running;  }

    ConnectionStatus GetConnectionStatus() const { return m_ConnectionStatus; }
    const std::string &GetConnectionDebugMessage() const { return m_ConnectionDebugMessage; }

private:
    void NetworkThreadFunc();
    void Shutdown();

    static void ConnectionstatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info);
    void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status);

    void PollIncomingMessages();
    void PollConnectionStateChanges();

    void OnFatalError(const std::string &message);

    std::thread m_NetworkThread;

    DataReceivedCallback m_DataReceivedCallback;
    ServerConnectedCallback m_ServerConnectedCallback;
    ServerDisconnectedCallback m_ServerDisconnectedCallback;

    ConnectionStatus m_ConnectionStatus = ConnectionStatus::Disconnected;
    std::string m_ConnectionDebugMessage;

    std::string m_ServerAddress;
    bool m_Running = false;

    ISteamNetworkingSockets *m_Interface = nullptr;
    HSteamNetConnection m_Connection = 0;
};

}

