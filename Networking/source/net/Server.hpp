#pragma once

#include "Buffer.hpp"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#include <string>
#include <map>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

namespace ignite::net
{
    using ClientID = HSteamNetConnection;

    struct ClientInfo
    {
        ClientID ID;
        std::string ConnectionDesc;
    };

    // Event produced on the network thread when the event queue is enabled.
    struct NetEvent
    {
        enum class Type : uint8_t { ClientConnected, ClientDisconnected, DataReceived };

        Type EventType = Type::DataReceived;
        ClientInfo Client{};
        std::vector<uint8_t> Data;
    };

    class Server
    {
    public:
        using DataReceivedCallback = std::function<void(const ClientInfo &, const Buffer)>;
        using ClientConnectedCallback = std::function<void(const ClientInfo &)>;
        using ClientDisconnectedCallback = std::function<void(const ClientInfo &)>;

    public:
        Server(int port);
        ~Server();

        void Start();
        void Stop();

        // When enabled, callbacks are not invoked on the network thread.
        // Events are queued and dispatched on the caller thread by PumpEvents().
        void EnableEventQueue(bool enable) { m_UseEventQueue = enable; }
        // Dispatch queued events to the registered callbacks. Returns the number of events processed.
        size_t PumpEvents();

        void SetDataReceivedCallback(const DataReceivedCallback &func);
        void SetClientConnectedCallback(const ClientConnectedCallback &func);
        void SetClientDisconnectedCallback(const ClientDisconnectedCallback &func);

        void SendBufferToClient(ClientID client_id, Buffer buffer, bool reliable = true);
        void SendStringToClient(ClientID client_id, const std::string &message, bool reliable);
        void SendBufferToAllClients(Buffer buffer, ClientID exclude_client_id = 0, bool reliable = true);
        void SendStringToClient(const std::string &message, ClientID exclude_client_id = 0, bool reliable = true);

        template<typename T>
        void SendDataToClient(ClientID client_id, const T &data, bool reliable = true)
        {
            SendBufferToClient(client_id, Buffer(&data, sizeof(T)), reliable);
        }

        template<typename T>
        void SetDataToAllClients(const T &data, ClientID exclude_client_id = 0, bool reliable = true)
        {
            SendBufferToAllClients(Buffer(&data, sizeof(T)), exclude_client_id, reliable);
        }

        void KickClient(ClientID client_id);

        bool IsRunning() const { return m_Running; }

        std::map<HSteamNetConnection, ClientInfo> GetConnectedClients() const
        {
            std::lock_guard<std::mutex> lock(m_ClientsMutex);
            return m_ConnectedClients;
        }
    private:

        // server thread
        void NetworkThreadFunc();

        static void ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *info);
        void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *status);

        void PollIncomingMessages();
        void SetClientNick(HSteamNetConnection h_connections, const char *nick);
        void PollConnectionStateChanges();

        void OnFatalError(const std::string &message);
        void PushEvent(NetEvent &&event);

    private:
        std::thread m_NetworkThread;
        DataReceivedCallback m_DataReceivedCallback;
        ClientConnectedCallback m_ClientConnectedCallback;
        ClientDisconnectedCallback m_ClientDisconnectedCallback;

        int m_Port = 0;
        std::atomic<bool> m_Running{ false };
        std::atomic<bool> m_UseEventQueue{ false };
        mutable std::mutex m_ClientsMutex;
        std::map<HSteamNetConnection, ClientInfo> m_ConnectedClients;

        std::mutex m_EventMutex;
        std::vector<NetEvent> m_Events;

        ISteamNetworkingSockets *m_Interface = nullptr;
        HSteamListenSocket m_ListenSocket = 0u;
        HSteamNetPollGroup m_PollGroup = 0u;
    };
}