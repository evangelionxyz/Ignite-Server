#include <iostream>
#include <format>
#include <mutex>
#include <map>

#include "net/Console.hpp"
#include "net/BufferStream.hpp"
#include "net/Buffer.hpp"
#include "net/ServerPacket.hpp"
#include "net/Server.hpp"
#include "net/ISerializable.hpp"

using namespace ignite;

static net::Buffer scratchBuffer;
static net::Console console;
static net::Server server{ 8192 };

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

std::map<uint32_t, Player> playerDataMap;
std::mutex loopMutex;

void OnConsoleMessage(std::string_view message)
{
    if (message.starts_with('/'))
    {
        std::cout << "You called the " << message << " command\n";
    }
}

void OnClientConnected(const net::ClientInfo &clientInfo)
{
    console.AddTaggedMessage("Server", "Client connected! ID={}", clientInfo.ID);

    // Initialize a new player for this client
    loopMutex.lock();
    {
        Player &newPlayer = playerDataMap[clientInfo.ID];
        newPlayer.id = clientInfo.ID;
        newPlayer.name = std::format("Player_{}", clientInfo.ID);
        newPlayer.posX = 0.0f;
        newPlayer.posY = 0.0f;
        newPlayer.posZ = 0.0f;
        newPlayer.level = 1;
    }
    loopMutex.unlock();

    net::BufferStreamWriter stream(scratchBuffer);
    stream.WriteRaw(net::PacketType::ClientConnect);
    stream.WriteRaw(clientInfo.ID);

    // Send Data to Client
    server.SendBufferToClient(clientInfo.ID, stream.GetBuffer());
}

void OnClientDisconected(const net::ClientInfo &clientInfo)
{
    console.AddTaggedMessage("Server", "Client disconnected! ID={}", clientInfo.ID);
    
    // Remove player data when client disconnects
    loopMutex.lock();
    {
        playerDataMap.erase(clientInfo.ID);
    }
    loopMutex.unlock();
}

void OnDataReceived(const net::ClientInfo &clientInfo, const net::Buffer buffer)
{
    net::BufferStreamReader stream(buffer);
    
    net::PacketType type;
    stream.ReadRaw(type);
    switch (type)
    {
        case net::PacketType::ClientUpdate:
        {
            loopMutex.lock();
            {
                Player &p = playerDataMap[clientInfo.ID];
                p.Deserialize(&stream);
                
                console.AddTaggedMessage("Server", "Player {} updated: pos({}, {}, {}) level={}", 
                    p.name, p.posX, p.posY, p.posZ, p.level);
            }
            loopMutex.unlock();
            break;
        }
    }
}

int main()
{
    // Initialize
    scratchBuffer.Allocate(10 * 1024 * 1024);

    server.SetClientConnectedCallback(OnClientConnected);
    server.SetClientDisconnectedCallback(OnClientDisconected);
    server.SetDataReceivedCallback(OnDataReceived);

    server.Start();

    bool running = true;

    while (running)
    {
        net::BufferStreamWriter stream(scratchBuffer);
        stream.WriteRaw(net::PacketType::ClientUpdate);
        loopMutex.lock();
        {
            // Write the number of players
            stream.WriteRaw<uint32_t>(static_cast<uint32_t>(playerDataMap.size()));
            
            // Serialize each player
            for (const auto &[clientID, player] : playerDataMap)
            {
                stream.WriteRaw<uint32_t>(clientID);
                player.Serialize(&stream);
            }
        }
        loopMutex.unlock();
        server.SendBufferToAllClients(stream.GetBuffer());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        running = server.IsRunning();
    }

    server.Stop();
}
