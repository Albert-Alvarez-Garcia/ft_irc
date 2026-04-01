#ifndef SERVER_HPP
#define SERVER_HPP

//---standar---//


#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <map>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <algorithm>

#include "Client.hpp"
#include "Channel.hpp"

/**
 * @class Server
 * @brief The core engine of the IRC server. 
 * Manages the connection lifecycle, socket multiplexing, and command routing.
 */
class Server {
private:
    // --- Server Configuration ---
    int         _port;
    std::string _password;
    int         _serverSocket;

    // --- Multiplexing & Data Storage ---
    /** @note _fds: Vector of pollfd structures for the poll() event loop. */
    std::vector<struct pollfd> _fds;
    /** @note _clients: Maps file descriptors (FD) to Client objects. */
    std::map<int, Client*> _clients;
    /** @note _channels: Maps channel names (e.g., "#42") to Channel objects. */
    std::map<std::string, Channel*> _channels;

    // --- INTERNAL NETWORK MANAGEMENT ---
    /** @note Accepts new TCP connections and sets them to non-blocking mode. */
    void acceptNewConnection();
    /** @note Reads data from an existing client socket into its internal buffer. */
    void handleClientData(size_t &i);
    /** @note Identifies complete IRC messages (\r\n) within the client's buffer. */
    void processBuffer(Client* client, size_t &i);
    /** @note Routes parsed messages to the appropriate command handler. */
    void executeCommand(Client* client, std::string message, size_t &i);
    /** @note Performs clean socket closure and removes client from all channels. */
    void disconnectClient(size_t &i);

    // --- UTILITY METHODS ---
    void    sendResponse(Client* client, std::string msg);
    Client* findClientByNick(std::string nick);

    // --- IRC COMMAND HANDLERS (The Brain) ---
    /** @note Registration Handshake */
    void handlePass(Client* client, std::string message);
    void handleNick(Client* client, std::string message);
    void handleUser(Client* client, std::string message);
    void sendWelcome(Client* client);

    /** @note Channel & Communication Operations */
    void handleJoin(Client* client, std::string message);
    void handlePrivmsg(Client* client, std::string message);
    void handleNotice(Client* client, std::string message);
    void handlePart(Client* client, std::string message);
    void handleTopic(Client* client, std::string message);
    void handleQuit(Client* client, std::string message, size_t &i);

    /** @note Operator & Privilege Commands (+i, +t, +k, +o, +l) */
    void handleMode(Client* client, std::string message);
    void handleKick(Client* client, std::string message);
    void handleInvite(Client* client, std::string message);
    
    /** @note Information Commands */
    void handleList(Client* client, std::string message);

    /** @note Bonus Features (Bot) */
    void checkBotCommands(Client* client, std::string target, std::string message);

public:
    Server(int port, std::string password);
    ~Server();

    /** @note Initializes the server socket (socket, setsockopt, bind, listen). */
    void init();
    /** @note Starts the main event loop (poll). */
    void run();
};

#endif