#ifndef SERVER_PACKET_HPP
#define SERVER_PACKET_HPP

#include <cstdint>

namespace ignite::net
{
    enum class PacketType : uint16_t
    {
        //
        // Invalid packet
        //
        None = 0,

        //
        // -- Message --
        // 
        // [Server->Client]
        // 1. Username - UTF-8 serialized as per Ignite
        // 2. Message - UTF-8 string serialized as per Ignite
        // [Client->Server]
        // 1. Message - buffer of UTF-8 chars
        Message = 1,

        // 
        // -- ClientConnectionRequest --
        // 
        // [Client->Server]
        // 1. 32-bit int with requested user color (RGB, most significant 8 bits ignored)
        // 2. Ignite serialized UTF-8 string with requested username
        // [Server->Client]
        // boolean response indicating acceptance of requested username
        ClientConnectionRequest = 2,

        // 
        // -- ConnectionStatus --
        // 
        // [Server->Client]
        // idk
        ConnectionStatus = 3,

        // 
        // -- ClientList --
        // 
        // [Server->Client]
        // Contains serialized std::vector (as per Ignite serialization) of ClientInfo structs (color + username)
        // This is received by client on connection, and at a set interval (10s default)
        ClientList = 4,

        // 
        // -- ClientConnect --
        // 
        // [Server->Client]
        // Indicates connection of new other client
        // 1. Requested user color (32-bit int RGB, most significant 8 bits ignored)
        // 2. Requested username (Ignite serialized UTF-8 string)
        ClientConnect = 5,

        // 
        // -- ClientUpdate --
        // 
        // [Server->Client]
        // 1. Existing username (Ignite serialized UTF-8 string)
        // 2. New color for user (32-bit int, RGB)
        // 3. New username (Ignite serialized UTF-8 string)
        // [Client->Server]
        // 1. Requested new color (32-bit int, RGB)
        // 2. Requested new username (Ignite serialized UTF-8 string)
        ClientUpdate = 6,

        // 
        // -- ClientDisconnect --
        // 
        // [Server->Client]
        // Indicates disconnection of existing other client
        // 1. Ignite serialized UTF-8 string with username
        // [Client->Server]
        // Disconnection request from client
        // 1. [No data]
        ClientDisconnect = 7,

        // 
        // -- ClientUpdateResponse --
        // 
        // [Server->Client]
        // 1. Boolean response for requested color
        // 2. Boolean response for requested username
        ClientUpdateResponse = 8,

        // 
        // -- MessageHistory --
        // 
        // [Server->Client]
        // Server chat history - big boi
        // 1. A vector of ChatMessage in order of send time
        MessageHistory = 9,

        // 
        // -- ServerShutdown --
        // 
        // [Server->Client]
        // Server is shutting down
        // <No data, just PacketType>
        ServerShutdown = 10,

        // 
        // -- ClientKick --
        // 
        // [Server->Client]
        // User has been kicked from server
        // 1. String reason, could be empty string
        ClientKick = 11,

        // -- PlayerInput --
        // [Client->Server] serialized player input for physics control
        PlayerInput = 12,

        // -- PhysicsState --
        // [Server->Client] authoritative physics state for replicated entities
        PhysicsState = 13,

        // -- EntitySpawn --
        // [Server->Client] request to instantiate an entity from template with a specific UUID
        EntitySpawn = 14,
    };

    static std::string_view PacketTypeToString(PacketType type)
    {
        switch (type)
        {
        case PacketType::None:                     return "PacketType::None";
        case PacketType::Message:                  return "PacketType::Message";
        case PacketType::ClientConnectionRequest:  return "PacketType::ClientConnectionRequest";
        case PacketType::ConnectionStatus:         return "PacketType::ConnectionStatus";
        case PacketType::ClientList:               return "PacketType::ClientList";
        case PacketType::ClientConnect:            return "PacketType::ClientConnect";
        case PacketType::ClientUpdate:             return "PacketType::ClientUpdate";
        case PacketType::ClientDisconnect:         return "PacketType::ClientDisconnect";
        case PacketType::ClientUpdateResponse:     return "PacketType::ClientUpdateResponse";
        case PacketType::MessageHistory:           return "PacketType::MessageHistory";
        case PacketType::ServerShutdown:           return "PacketType::ServerShutdown";
        case PacketType::ClientKick:               return "PacketType::ClientKick";
        case PacketType::PlayerInput:              return "PacketType::PlayerInput";
        case PacketType::PhysicsState:             return "PacketType::PhysicsState";
        case PacketType::EntitySpawn:              return "PacketType::EntitySpawn";

        default: return "PacketType::<Invalid>";
        }

        return "PacketType::<Invalid>";
    }
}

#endif