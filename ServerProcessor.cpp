#include "Server.hpp"

void Server::processBuffer(Client* client, size_t &i) 
{
    std::string clientBuf = client->getBuffer();
    size_t pos;
    int currentFd = _fds[i].fd; // Store FD to verify if client is still alive

    // --- DEBUG BLOCK ---
    if (!clientBuf.empty()) 
    {
        std::cout << "[DEBUG] Received from " << client->getNickname() << ": [";
        for (size_t j = 0; j < clientBuf.size(); j++) 
        {
            if (clientBuf[j] == '\r') std::cout << "\\r";
            else if (clientBuf[j] == '\n') std::cout << "\\n";
            else if (clientBuf[j] == ' ') std::cout << "(space)";
            else std::cout << clientBuf[j];
        }
        std::cout << "]" << std::endl;
    }

    while ((pos = clientBuf.find("\n")) != std::string::npos) 
    {
        std::string message = clientBuf.substr(0, pos);
        clientBuf.erase(0, pos + 1);
        
        // IMPORTANT: Update client buffer BEFORE executing the command
        client->setBuffer(clientBuf);

        // Aggressive trimming
        while (!message.empty() && (unsigned char)message[message.size() - 1] <= 32)
            message.erase(message.size() - 1);
        while (!message.empty() && (unsigned char)message[0] <= 32)
            message.erase(0, 1);

        if (message.empty()) continue;

        // EXECUTION
        executeCommand(client, message, i);

        // SAFETY CHECK
        // If client is no longer in the map (QUIT), exit immediately.
        // We stop here to avoid accessing the 'client' pointer again.
        if (_clients.find(currentFd) == _clients.end()) 
        {
            return; 
        }

        // Refresh clientBuf in case the previous command left pending data
        clientBuf = client->getBuffer();
    }

    // Emergency exit for QUIT without \n (Only if client is still alive)
    if (!clientBuf.empty()) 
    {
        std::string checkQuit = clientBuf;
        while (!checkQuit.empty() && (unsigned char)checkQuit[checkQuit.size() - 1] <= 32)
            checkQuit.erase(checkQuit.size() - 1);
        
        if (checkQuit == "QUIT") 
        {
            executeCommand(client, "QUIT", i);
            return;
        }
    }
    
    // Finally, save remaining data only if the client survived the commands
    client->setBuffer(clientBuf);
}

void Server::executeCommand(Client* client, std::string message, size_t &i) 
{
    // 1. PRE-CLEANUP: Remove spaces and junk from start and end of raw message
    while (!message.empty() && (unsigned char)message[message.size() - 1] <= 32)
        message.erase(message.size() - 1);
    while (!message.empty() && (unsigned char)message[0] <= 32)
        message.erase(0, 1);

    if (message.empty()) return;

    // 2. COMMAND EXTRACTION
    std::stringstream ss(message);
    std::string cmd;
    if (!(ss >> cmd)) return; 
    for (size_t j = 0; j < cmd.size(); j++) cmd[j] = std::toupper(cmd[j]);

    // 3. TOP PRIORITY: QUIT
    // Due to the previous cleanup, this will catch QUIT even with accidental spaces
    
    if (cmd == "CAP") 
    {
        // Ignore CAP messages from other IRC clients to avoid "Unknown Command"
        return; 
    }
    if (cmd == "QUIT") 
    {
        handleQuit(client, message, i);
        return; 
    }

    // 4. REGISTRATION COMMANDS
    if (cmd == "PASS") 
    {
        handlePass(client, message);
    } 
    else if (cmd == "NICK") 
    {
        handleNick(client, message);
    } 
    else if (cmd == "USER") 
    {
        handleUser(client, message);
    }
    
    // 5. REGISTRATION FILTER
    else if (!client->isRegistered()) 
    {
        sendResponse(client, ":ircserv 451 * :You have not registered\n");
    }
    
    // 6. OPERATIONAL COMMANDS
    else if (cmd == "PRIVMSG") handlePrivmsg(client, message);
    else if (cmd == "JOIN")    handleJoin(client, message);
    else if (cmd == "INVITE")  handleInvite(client, message);
    else if (cmd == "TOPIC")   handleTopic(client, message);
    else if (cmd == "KICK")    handleKick(client, message);
    else if (cmd == "PART")    handlePart(client, message);
    else if (cmd == "LIST")    handleList(client, message);
    else if (cmd == "MODE")    handleMode(client, message);
    else if (cmd == "NOTICE") handleNotice(client, message);
    else if (cmd == "PING") 
    {
        std::string token;
        ss >> token;
        sendResponse(client, ":ircserv PONG ircserv " + token + "\n");
    }
    
    // 7. UNKNOWN COMMAND
    else 
    {
        sendResponse(client, ":ircserv 421 " + client->getNickname() + " " + cmd + " :Unknown command\n");
    }
}