#include <iostream>
#include <format>
#include <mutex>
#include <map>
#include <sstream>
#include <thread>

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

struct ChatMessage
{
    uint32_t clientID;
    std::string username;
    std::string message;
    std::string timestamp;
};

std::vector<ChatMessage> chatHistory;
std::mutex chatMutex;

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

std::string GetCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    return std::format("{:02d}:{:02d}:{:02d}", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void BroadcastChatMessage(const ChatMessage& msg)
{
    net::BufferStreamWriter stream(scratchBuffer);
    stream.WriteRaw(net::PacketType::Message);
    stream.WriteString(msg.username);
    stream.WriteString(msg.message);
    stream.WriteString(msg.timestamp);
    server.SendBufferToAllClients(stream.GetBuffer());
    
    console.AddTaggedMessage("Chat", "[{}] {}: {}", msg.timestamp, msg.username, msg.message);
}

void ProcessServerCommand(std::string_view message)
{
    std::istringstream iss{std::string(message)};
    std::string command;
    iss >> command;

    if (command == "/kick")
    {
        uint32_t clientID;
        if (iss >> clientID)
        {
            loopMutex.lock();
            auto it = playerDataMap.find(clientID);
            if (it != playerDataMap.end())
            {
                std::string playerName = it->second.name;
                loopMutex.unlock();
                
                // Send kick notification to client
                net::BufferStreamWriter stream(scratchBuffer);
                stream.WriteRaw(net::PacketType::ClientKick);
                stream.WriteString(std::string("Kicked by server admin"));
                server.SendBufferToClient(clientID, stream.GetBuffer());
                
                // Disconnect the client
                server.KickClient(clientID);
                
                console.AddTaggedMessage("Server", "Kicked player {} (ID: {})", playerName, clientID);
            }
            else
            {
                loopMutex.unlock();
                console.AddTaggedMessage("Server", "Error: Client ID {} not found", clientID);
            }
        }
        else
        {
            console.AddTaggedMessage("Server", "Usage: /kick <client_id>");
        }
    }
    else if (command == "/list")
    {
        loopMutex.lock();
        console.AddTaggedMessage("Server", "Connected clients ({}): ", playerDataMap.size());
        for (const auto& [id, player] : playerDataMap)
        {
            console.AddTaggedMessage("Server", "  ID: {} - {} (Level {})", id, player.name, player.level);
        }
        loopMutex.unlock();
    }
    else if (command == "/shutdown")
    {
        console.AddTaggedMessage("Server", "Shutting down server...");
        
        // Notify all clients
        net::BufferStreamWriter stream(scratchBuffer);
        stream.WriteRaw(net::PacketType::ServerShutdown);
        server.SendBufferToAllClients(stream.GetBuffer());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        server.Stop();
    }
    else if (command == "/help")
    {
        console.AddTaggedMessage("Server", "Available commands:");
        console.AddTaggedMessage("Server", "  /kick <id> - Kick a client by ID");
        console.AddTaggedMessage("Server", "  /list - List all connected clients");
        console.AddTaggedMessage("Server", "  /shutdown - Shutdown the server");
        console.AddTaggedMessage("Server", "  /help - Show this help message");
    }
    else
    {
        console.AddTaggedMessage("Server", "Unknown command: {}. Type /help for available commands.", command);
    }
}

void OnConsoleMessage(std::string_view message)
{
    if (message.starts_with('/'))
    {
        ProcessServerCommand(message);
    }
    else if (!message.empty())
    {
        // Server broadcast message
        ChatMessage msg;
        msg.clientID = 0;
        msg.username = "Server";
        msg.message = std::string(message);
        msg.timestamp = GetCurrentTimestamp();
        
        chatMutex.lock();
        chatHistory.push_back(msg);
        chatMutex.unlock();
        
        BroadcastChatMessage(msg);
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
        case net::PacketType::Message:
        {
            std::string message;
            stream.ReadString(message);
            
            loopMutex.lock();
            std::string username = playerDataMap[clientInfo.ID].name;
            loopMutex.unlock();
            
            ChatMessage chatMsg;
            chatMsg.clientID = clientInfo.ID;
            chatMsg.username = username;
            chatMsg.message = message;
            chatMsg.timestamp = GetCurrentTimestamp();
            
            chatMutex.lock();
            chatHistory.push_back(chatMsg);
            chatMutex.unlock();
            
            BroadcastChatMessage(chatMsg);
            break;
        }
        case net::PacketType::ClientUpdate:
        {
            loopMutex.lock();
            {
                Player &p = playerDataMap[clientInfo.ID];
                p.Deserialize(&stream);
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

    console.SetMessageSendCallback(OnConsoleMessage);
    server.SetClientConnectedCallback(OnClientConnected);
    server.SetClientDisconnectedCallback(OnClientDisconected);
    server.SetDataReceivedCallback(OnDataReceived);

    server.Start();
    console.AddTaggedMessage("Server", "Server started on port 8192");
    console.AddTaggedMessage("Server", "Type /help for available commands");

    bool running = true;

    while (running)
    {
        // Send player updates to all clients periodically
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
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        running = server.IsRunning();
    }

    console.AddTaggedMessage("Server", "Server stopped");
    server.Stop();
}
