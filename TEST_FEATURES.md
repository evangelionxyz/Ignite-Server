# Test Client/Server Features

## Overview
Enhanced test programs for the Ignite-Server with real-time chat messaging and server administration commands.

## Server Features

### Chat System
- Real-time message broadcasting to all connected clients
- Message history with timestamps
- Server can send broadcast messages to all clients

### Server Commands
Type these commands in the server console:

- `/kick <id>` - Kick a client by their ID
- `/list` - List all connected clients with their IDs and names
- `/shutdown` - Gracefully shutdown the server (notifies all clients)
- `/help` - Display available commands

### Player Management
- Automatic player creation on client connection
- Player data synchronization (position, level, name)
- Clean disconnect handling

## Client Features

### Chat System
- Send chat messages to all connected clients
- Receive messages from server and other clients
- Messages displayed with username and timestamp

### Client Commands
Type these commands in the client console:

- `/help` - Show available commands
- `/quit` - Disconnect from server
- `/players` - List all connected players

### Real-time Updates
- Continuous player position/data synchronization
- Receive notifications when kicked or server shuts down
- Non-blocking input handling with separate input thread

## Protocol

### New Packet Types Used
- `Message` - Chat messages between server and clients
- `ClientKick` - Server kicks a client with optional reason
- `ServerShutdown` - Server notifies clients of shutdown

### Player Data
Each player has:
- `id` - Unique client ID assigned by server
- `name` - Player name (auto-generated as Player_<ID>)
- `posX, posY, posZ` - Player position (float)
- `level` - Player level (uint16_t)

## Running the Tests

### Start the Server
```bash
./build/TestServer/TestServer
```

### Start Client(s)
```bash
./build/TestClient/TestClient
```

You can start multiple clients to test multi-user chat and player management.

## Example Usage

**Server Console:**
```
> /list
> Hello everyone!
> /kick 1
> /shutdown
```

**Client Console:**
```
> Hello from client!
> /players
> /quit
```

## Technical Details

- Messages are sent using `PacketType::Message` with username, message content, and timestamp
- Server processes commands in a dedicated function `ProcessServerCommand()`
- Client uses a separate input thread to prevent blocking during network operations
- All player data is synchronized every 100ms
- Thread-safe message queues and mutexes for concurrent access
