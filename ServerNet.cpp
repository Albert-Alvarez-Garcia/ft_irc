#include "Server.hpp"

void Server::acceptNewConnection() 
{
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(_serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket == -1) return;

    fcntl(clientSocket, F_SETFL, O_NONBLOCK);
    struct pollfd client_pollfd = {clientSocket, POLLIN, 0};
    _fds.push_back(client_pollfd);
    _clients[clientSocket] = new Client(clientSocket);
    std::cout << "[LOG] New client connected on FD: " << clientSocket << std::endl;
}

void Server::handleClientData(size_t &i) 
{
    char buffer[1024];
    // 1. Full buffer cleanup
    std::memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytesRead = recv(_fds[i].fd, buffer, sizeof(buffer) - 1, 0);

    // 2. Strict disconnection control
    if (bytesRead <= 0) {
        std::cout << "[LOG] Client on FD " << _fds[i].fd << " disconnected." << std::endl;
        disconnectClient(i); 
        /* Important: disconnectClient must decrement 'i' if the element is removed 
           from the vector to prevent the main loop from skipping the next client. */
        return;
    }

    // 3. Retrieve client safely
    if (_clients.find(_fds[i].fd) == _clients.end()) return;
    Client* client = _clients[_fds[i].fd];

    // 4. Add only read bytes (avoids reading garbage from RAM)
    std::string dataReceived(buffer, bytesRead);
    client->addToBuffer(dataReceived);

    // 5. Process buffer
    processBuffer(client, i);
}

void Server::disconnectClient(size_t &i) 
{
    int fd = _fds[i].fd;
    Client* client = _clients[fd];
    if (client) {
        std::string nick = client->getNickname(); // <--- Store nickname here
        if (nick.empty()) nick = "Guest"; 

        std::string qMsg = ":" + nick + " QUIT :Client Quit\n";
        
        // Broadcast using the stored nickname
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ) 
        {
            it->second->broadcast(qMsg, client);
            it->second->removeMember(client);
            if (it->second->getMembers().empty()) 
            {
                delete it->second;
                _channels.erase(it++);
            } else {
                ++it;
            }
        }
        delete client;
        _clients.erase(fd);
    }
    close(fd);
    _fds.erase(_fds.begin() + i);
    i--; // Decrement index to account for vector shift
}

void Server::sendResponse(Client* client, std::string msg) 
{
    send(client->getFd(), msg.c_str(), msg.length(), 0);
}

Client* Server::findClientByNick(std::string nick) 
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        if (it->second->getNickname() == nick) return it->second;
    return NULL;
}
