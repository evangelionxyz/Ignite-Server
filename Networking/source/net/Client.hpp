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
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

namespace ignite::net
{
    class Client
    {
    public:
        enum class ConnectionStatus
        {
            Disconnected = 0, Connected, Connecting, FailedToConnect
        };
        
        // Event produced on the network thread when the event queue is enabled.
        struct NetEvent
        {
            enum class Type : uint8_t { Connected, Disconnected, DataReceived };

            Type EventType = Type::DataReceived;
            std::vector<uint8_t> Data;
        };

        using DataReceivedCallback = std::function<void(const Buffer)>;
        using ServerConnectedCallback = std::function<void()>;
        using ServerDisconnectedCallback = std::function<void()>;

        Client() = default;
        ~Client();

        void ConnectToServer(const std::string &server_address);
        void Disconnect();

        // When enabled, callbacks are not invoked on the network thread.
        // Events are queued and dispatched on the caller thread by PumpEvents().
        void EnableEventQueue(bool enable) { m_UseEventQueue = enable; }
        // Dispatch queued events to the registered callbacks. Returns the number of events processed.
        size_t PumpEvents();

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
        HSteamNetConnection GetConnectionHandle() const { return m_Connection; }
        bool GetRealTimeStatus(SteamNetConnectionRealTimeStatus_t *status) const;

    private:
        void NetworkThreadFunc();
        void Shutdown();

        static void ConnectionstatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info);
        void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status);

        void PollIncomingMessages();
        void PollConnectionStateChanges();

        void OnFatalError(const std::string &message);
        void PushEvent(NetEvent &&event);

        std::thread m_NetworkThread;

        DataReceivedCallback m_DataReceivedCallback;
        ServerConnectedCallback m_ServerConnectedCallback;
        ServerDisconnectedCallback m_ServerDisconnectedCallback;

        std::atomic<ConnectionStatus> m_ConnectionStatus{ ConnectionStatus::Disconnected };
        std::atomic<bool> m_UseEventQueue{ false };
        std::mutex m_EventMutex;
        std::vector<NetEvent> m_Events;
        std::string m_ConnectionDebugMessage;

        std::string m_ServerAddress;
        std::atomic<bool> m_Running{ false };

        ISteamNetworkingSockets *m_Interface = nullptr;
        HSteamNetConnection m_Connection = 0;
    };

}