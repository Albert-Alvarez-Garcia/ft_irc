#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>

Server::Server(int port, std::string password) : _port(port), _password(password), _serverSocket(-1) {
    std::cout << "Servidor creado para el puerto " << _port << std::endl;
}

Server::~Server() {
    if (_serverSocket != -1) close(_serverSocket);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
}

// --- INICIALIZACIÓN Y BUCLE ---

void Server::init() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket == -1) throw std::runtime_error("Error al crear el socket");

    int en = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) == -1) 
        throw std::runtime_error("Error en setsockopt");

    if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) == -1) 
        throw std::runtime_error("Error en fcntl");

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_serverSocket, (struct sockaddr *)&address, sizeof(address)) == -1) 
        throw std::runtime_error("Error en bind");

    if (listen(_serverSocket, SOMAXCONN) == -1) 
        throw std::runtime_error("Error en listen");

    std::cout << "Servidor escuchando en el puerto " << _port << "..." << std::endl;
}

void Server::run() {
    struct pollfd v_server_fd;
    v_server_fd.fd = _serverSocket;
    v_server_fd.events = POLLIN;
    v_server_fd.revents = 0;
    _fds.push_back(v_server_fd);

    //quitamos este poll por la necesidad de un 'unico poll en el proyecto a pesar de ser este de seguridad por si no se entra en el bucle

    /*
    int poll_count = poll(&_fds[0], _fds.size(), 1000); 
    
    if (poll_count < 0) {
        // Si poll falla por una señal (Ctrl+C), no es un error real
        return;
    }
*/
    while (true) {
        // Usamos &_fds[0] para pasar el puntero al array interno
        if (poll(&_fds[0], _fds.size(), -1) == -1) {
            // Si el servidor se cierra por una señal, evitamos el throw
            break; 
        }

        for (size_t i = 0; i < _fds.size(); i++) {
            if (_fds[i].revents & POLLIN) {
                if (_fds[i].fd == _serverSocket) {
                    acceptNewConnection();
                } else {
                    // Guardamos el FD actual para comparar después
                    int old_fd = _fds[i].fd;
                    
                    handleClientData(i);

                    // --- EL FIX CRÍTICO ---
                    // Si el cliente se desconectó en handleClientData (QUIT), 
                    // el vector _fds ha encogido y los elementos se han desplazado.
                    // Verificamos si el elemento en la posición 'i' ya no es el mismo.
                    if (i < _fds.size() && _fds[i].fd != old_fd) {
                        i--; // Retrocedemos el índice para no saltarnos al siguiente cliente
                    } else if (i >= _fds.size()) {
                        // Si era el último elemento y se borró, salimos del for
                        break;
                    }
                }
            }
        }
    }
}

// --- GESTIÓN DE RED ---

void Server::acceptNewConnection() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(_serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket == -1) return;

    fcntl(clientSocket, F_SETFL, O_NONBLOCK);
    struct pollfd client_pollfd = {clientSocket, POLLIN, 0};
    _fds.push_back(client_pollfd);
    _clients[clientSocket] = new Client(clientSocket);
    std::cout << "Nuevo cliente conectado: " << clientSocket << std::endl;
}

void Server::handleClientData(size_t &i) {
    char buffer[1024];
    // 1. Limpieza total del buffer
    std::memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytesRead = recv(_fds[i].fd, buffer, sizeof(buffer) - 1, 0);

    // 2. Control estricto de desconexión
    if (bytesRead <= 0) {
        std::cout << "[LOG] Cliente en FD " << _fds[i].fd << " desconectado." << std::endl;
        disconnectClient(i); 
        // Importante: disconnectClient debe restar 1 a 'i' si borras el elemento del vector
        // para que el bucle del main no se salte al siguiente cliente.
        return;
    }

    // 3. Obtener el cliente de forma segura
    if (_clients.find(_fds[i].fd) == _clients.end()) return;
    Client* client = _clients[_fds[i].fd];

    // 4. Añadir solo los bytes leídos (evita leer basura de la RAM)
    std::string dataReceived(buffer, bytesRead);
    client->addToBuffer(dataReceived);

    // 5. Procesar
    processBuffer(client, i);
}


// --- COMANDOS DE REGISTRO ---



// --- COMANDOS OPERATIVOS ---




// --- LIMPIEZA ---

void Server::disconnectClient(size_t &i) {
    int fd = _fds[i].fd;
    Client* client = _clients[fd];
    if (client) {
        std::string nick = client->getNickname(); // <--- Guardamos el nick aquí
        if (nick.empty()) nick = "Guest"; 

        std::string qMsg = ":" + nick + " QUIT :Client Quit\n";
        
        // Ahora hacemos el broadcast con ese nick guardado
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ) {
            it->second->broadcast(qMsg, client);
            it->second->removeMember(client);
            if (it->second->getMembers().empty()) {
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
    i--;
}
void Server::sendResponse(Client* client, std::string msg) {
    send(client->getFd(), msg.c_str(), msg.length(), 0);
}

Client* Server::findClientByNick(std::string nick) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        if (it->second->getNickname() == nick) return it->second;
    return NULL;
}
