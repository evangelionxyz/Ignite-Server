#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <thread>
#include <sstream>
#include <queue>

#include "net/BufferStream.hpp"
#include "net/ServerPacket.hpp"
#include "net/ISerializable.hpp"
#include "net/Client.hpp"

using namespace ignite;
net::Buffer scratchBuffer;
net::Client client;
std::mutex mutex;

bool running = true;
std::queue<std::string> inputQueue;
std::mutex inputMutex;

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
        case net::PacketType::Message:
        {
            std::string username, message, timestamp;
            stream.ReadString(username);
            stream.ReadString(message);
            stream.ReadString(timestamp);
            std::cout << "[" << timestamp << "] " << username << ": " << message << std::endl;
            std::cout << "> " << std::flush;  // Redisplay prompt
            break;
        }
        case net::PacketType::ClientConnect:
        {
            uint32_t idFromServer;
            stream.ReadRaw<uint32>(idFromServer);
            player.id = idFromServer;
            player.name = "Player_" + std::to_string(player.id);
            std::cout << "Connected to server with Player ID: " << player.id << std::endl;
            std::cout << "Type messages to chat, or /help for commands" << std::endl;
            std::cout << "> " << std::flush;
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
        case net::PacketType::ClientKick:
        {
            std::string reason;
            stream.ReadString(reason);
            std::cout << "\n*** You have been kicked from the server ***" << std::endl;
            if (!reason.empty())
            {
                std::cout << "Reason: " << reason << std::endl;
            }
            running = false;
            break;
        }
        case net::PacketType::ServerShutdown:
        {
            std::cout << "\n*** Server is shutting down ***" << std::endl;
            running = false;
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

void InputThread()
{
    std::string line;
    while (running)
    {
        if (std::getline(std::cin, line))
        {
            if (!line.empty())
            {
                inputMutex.lock();
                inputQueue.push(line);
                inputMutex.unlock();
            }
        }
        else
        {
            // Handle EOF or error
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void ProcessClientCommand(const std::string& message)
{
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    if (command == "/help")
    {
        std::cout << "Available commands:" << std::endl;
        std::cout << "  /help - Show this help message" << std::endl;
        std::cout << "  /quit - Disconnect from server" << std::endl;
        std::cout << "  /players - List connected players" << std::endl;
        std::cout << "  Anything else will be sent as a chat message" << std::endl;
    }
    else if (command == "/quit")
    {
        std::cout << "Disconnecting from server..." << std::endl;
        running = false;
    }
    else if (command == "/players")
    {
        mutex.lock();
        std::cout << "Connected players (" << playerDataMap.size() << "):" << std::endl;
        for (const auto& [id, p] : playerDataMap)
        {
            std::cout << "  ID: " << id << " - " << p.name;
            if (id == player.id)
                std::cout << " (You)";
            std::cout << std::endl;
        }
        mutex.unlock();
    }
    else
    {
        std::cout << "Unknown command: " << command << ". Type /help for available commands." << std::endl;
    }
}

int main()
{
    scratchBuffer.Allocate(10 * 1024 * 1024);
    client.SetDataReceivedCallback(OnDataReceived);
    client.SetServerConnectedCallback(OnServerConnected);
    client.SetServerDisconnectedCallback(OnServerDisconnected);

    std::cout << "Connecting to server at 127.0.0.1:8192..." << std::endl;
    client.ConnectToServer("127.0.0.1:8192");

    // Create Player data
    player.id = uint32_t(-1);
    player.posX = 0.0f;
    player.posY = 0.0f;
    player.posZ = 0.0f;
    player.level = 1;

    // Start input thread
    std::thread inputThread(InputThread);
    inputThread.detach();

    while (running)
    {
        auto connStatus = client.GetConnectionStatus();
        
        if (connStatus == net::Client::ConnectionStatus::Connected)
        {
            // Process any pending input
            inputMutex.lock();
            if (!inputQueue.empty())
            {
                std::string input = inputQueue.front();
                inputQueue.pop();
                inputMutex.unlock();

                if (input.starts_with('/'))
                {
                    ProcessClientCommand(input);
                }
                else
                {
                    // Send chat message to server
                    net::BufferStreamWriter stream(scratchBuffer);
                    stream.WriteRaw(net::PacketType::Message);
                    stream.WriteString(input);
                    client.SendBuffer(stream.GetBuffer());
                }
                
                if (running)
                    std::cout << "> " << std::flush;
            }
            else
            {
                inputMutex.unlock();
            }

            // Send player update
            net::BufferStreamWriter stream(scratchBuffer);
            stream.WriteRaw(net::PacketType::ClientUpdate);
            player.Serialize(&stream);
            client.SendBuffer(stream.GetBuffer());
        }
        else if (connStatus == net::Client::ConnectionStatus::Connecting)
        {
            // Still connecting, wait a bit
        }
        else
        {
            // Connection failed or disconnected
            if (running)
            {
                std::cout << "Connection lost. Status: " << int(connStatus) << std::endl;
                running = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    client.Disconnect();
    std::cout << "Disconnected from server." << std::endl;

    return 0;
}