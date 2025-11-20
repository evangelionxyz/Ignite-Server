#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <mutex>
#include <iostream>

#include "net/BufferStream.hpp"
#include "net/ServerPacket.hpp"
#include "net/ISerializable.hpp"
#include "net/Client.hpp"

using namespace ignite;
net::Buffer scratchBuffer;
net::Client client;
std::mutex mutex;

bool running = true;

class Player : public net::ISerializable
{
public:
    uint32_t id;
    std::string name;
    float posX, posY, posZ;
    uint16_t level;

    Player() : id(0), posX(0.0f), posY(0.0f), posZ(0.0f), level(1) {}

    // Implement ISerializable interface
    void Serialize(net::StreamWriter* writer) const override
    {
        writer->WriteRaw<uint32_t>(id);
        writer->WriteString(name);
        writer->WriteRaw<float>(posX);
        writer->WriteRaw<float>(posY);
        writer->WriteRaw<float>(posZ);
        writer->WriteRaw<uint16_t>(level);
    }

    void Deserialize(net::StreamReader* reader) override
    {
        reader->ReadRaw<uint32_t>(id);
        reader->ReadString(name);
        reader->ReadRaw<float>(posX);
        reader->ReadRaw<float>(posY);
        reader->ReadRaw<float>(posZ);
        reader->ReadRaw<uint16_t>(level);
    }
};

Player player;
std::map<uint32_t, Player> playerDataMap;

void OnDataReceived(net::Buffer buffer)
{
    net::BufferStreamReader stream(buffer);
    net::PacketType type;
    stream.ReadRaw(type);

    switch (type)
    {
        case net::PacketType::ClientConnect:
        {
            uint32_t idFromServer;
            stream.ReadRaw<uint32>(idFromServer);
            player.id = idFromServer;
			std::cout << "Connected to server with Player ID: " << player.id << std::endl;
            break;
        }
        case net::PacketType::ClientUpdate:
        {
            mutex.lock();
            {
                stream.ReadMap(playerDataMap);
            }
            mutex.unlock();
            break;
        }
        case net::PacketType::ServerShutdown:
        {
            std::cout << "Server is shutting down." << std::endl;
            break;
		}
    }
}

void OnServerConnected()
{

}

void OnServerDisconnected()
{
    auto it = playerDataMap.find(player.id);
    if (it != playerDataMap.end())
        playerDataMap.erase(it);
}

int main()
{
    scratchBuffer.Allocate(10 * 1024 * 1024);
    client.SetDataReceivedCallback(OnDataReceived);
    client.SetServerConnectedCallback(OnServerConnected);
    client.SetServerDisconnectedCallback(OnServerDisconnected);

    client.ConnectToServer("127.0.0.1:8192");

    // Create Player data
    player.id = uint32_t(-1);

    while (running)
    {
        auto connStatus = client.GetConnectionStatus();
        if (connStatus == net::Client::ConnectionStatus::Connected)
        {
            net::BufferStreamWriter stream(scratchBuffer);
            stream.WriteRaw(net::PacketType::ClientUpdate);
            stream.WriteData((const char *) &player, sizeof(player));
            client.SendBuffer(stream.GetBuffer());

            mutex.lock();
            std::map<uint32_t, Player> playerData = playerDataMap;
            mutex.unlock();

            for (const auto& [id, data] : playerData)
            {
                // Skip update current player
                if (id == player.id)
                    continue;

                // Update other player
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        running = connStatus == net::Client::ConnectionStatus::Connected || 
			connStatus == net::Client::ConnectionStatus::Connecting;
    }

    client.Disconnect();

    return 0;
}