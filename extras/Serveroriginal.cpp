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

void Server::processBuffer(Client* client, size_t &i) {
    std::string clientBuf = client->getBuffer();
    size_t pos;
    int currentFd = _fds[i].fd; // Guardamos el FD para verificar vida

    // --- BLOQUE DEBUGGER  ---
    if (!clientBuf.empty()) {
        std::cout << "[DEBUG] Recibido de " << client->getNickname() << ": [";
        for (size_t j = 0; j < clientBuf.size(); j++) {
            if (clientBuf[j] == '\r') std::cout << "\\r";
            else if (clientBuf[j] == '\n') std::cout << "\\n";
            else if (clientBuf[j] == ' ') std::cout << "(espacio)";
            else std::cout << clientBuf[j];
        }
        std::cout << "]" << std::endl;
    }

    while ((pos = clientBuf.find("\n")) != std::string::npos) {
        std::string message = clientBuf.substr(0, pos);
        clientBuf.erase(0, pos + 1);
        
        // IMPORTANTE: Actualizamos el buffer del cliente ANTES de ejecutar el comando
        client->setBuffer(clientBuf);

        // Limpieza agresiva
        while (!message.empty() && (unsigned char)message[message.size() - 1] <= 32)
            message.erase(message.size() - 1);
        while (!message.empty() && (unsigned char)message[0] <= 32)
            message.erase(0, 1);

        if (message.empty()) continue;

        // EJECUTAMOS
        executeCommand(client, message, i);

        // VERIFICACIÓN DE SEGURIDAD
        // Si el cliente ya no está en el mapa (QUIT), salimos YA.
        // No llegamos al final de la función para no tocar 'client' de nuevo.
        if (_clients.find(currentFd) == _clients.end()) {
            return; 
        }

        // Refrescamos clientBuf por si el comando anterior dejó algo pendiente
        clientBuf = client->getBuffer();
    }

    // Salida de emergencia para QUIT sin \n (Solo si el cliente sigue vivo)
    if (!clientBuf.empty()) {
        std::string checkQuit = clientBuf;
        while (!checkQuit.empty() && (unsigned char)checkQuit[checkQuit.size() - 1] <= 32)
            checkQuit.erase(checkQuit.size() - 1);
        
        if (checkQuit == "QUIT") {
            executeCommand(client, "QUIT", i);
            return;
        }
    }
    
    // Al final, solo si el cliente sobrevivió a los comandos, guardamos lo que quede
    client->setBuffer(clientBuf);
}
void Server::executeCommand(Client* client, std::string message, size_t &i) {
    // 1. LIMPIEZA PREVIA: Eliminamos espacios y basura al inicio y al final del mensaje bruto
    while (!message.empty() && (unsigned char)message[message.size() - 1] <= 32)
        message.erase(message.size() - 1);
    while (!message.empty() && (unsigned char)message[0] <= 32)
        message.erase(0, 1);

    if (message.empty()) return;

    // 2. EXTRACCIÓN DEL COMANDO
    std::stringstream ss(message);
    std::string cmd;
    if (!(ss >> cmd)) return; 
    for (size_t j = 0; j < cmd.size(); j++) cmd[j] = std::toupper(cmd[j]);

    // 3. PRIORIDAD MÁXIMA: QUIT
    // Con la limpieza anterior, aquí entrará aunque hayas puesto un espacio por error
    
    if (cmd == "CAP") {
    // No hacemos nada. Ignoramos el mensaje de otros irc para que no salte el "Unknown Command"
    return; 
    }
    if (cmd == "QUIT") {
        handleQuit(client, message, i);
        return; 
    }

    // 4. COMANDOS DE REGISTRO
    if (cmd == "PASS") {
        handlePass(client, message);
    } 
    else if (cmd == "NICK") {
        handleNick(client, message);
    } 
    else if (cmd == "USER") {
        handleUser(client, message);
    }
    
    // 5. FILTRO DE REGISTRO
    else if (!client->isRegistered()) {
        sendResponse(client, ":ircserv 451 * :You have not registered\n");
    }
    
    // 6. COMANDOS OPERATIVOS
    else if (cmd == "PRIVMSG") handlePrivmsg(client, message);
    else if (cmd == "JOIN")    handleJoin(client, message);
    else if (cmd == "INVITE")  handleInvite(client, message);
    else if (cmd == "TOPIC")   handleTopic(client, message);
    else if (cmd == "KICK")    handleKick(client, message);
    else if (cmd == "PART")    handlePart(client, message);
    else if (cmd == "LIST")    handleList(client, message);
    else if (cmd == "MODE")    handleMode(client, message);
    else if (cmd == "NOTICE") handleNotice(client, message);
    else if (cmd == "PING") {
    std::string token;
    ss >> token;
    sendResponse(client, ":ircserv PONG ircserv " + token + "\n");
}
    
    // 7. COMANDO DESCONOCIDO
    else {
        sendResponse(client, ":ircserv 421 " + client->getNickname() + " " + cmd + " :Unknown command\n");
    }
}
// --- COMANDOS DE REGISTRO ---

void Server::handlePass(Client* client, std::string message) {
    if (message.length() < 6) return;
    if (message.substr(5) == _password) {
        client->setAuthenticated(true);
        sendResponse(client, "PASS: Password accepted.\n");
    } else {
        sendResponse(client, "PASS: Wrong password.\n");
    }
}

void Server::handleNick(Client* client, std::string message) {
    if (!client->isAuthenticated()) {
        sendResponse(client, "ERROR: Send PASS first.\n");
        return;
    }
    std::string nuevo_nick = message.substr(5);
    size_t p_n = nuevo_nick.find_first_of(" \r\n");
    if (p_n != std::string::npos) nuevo_nick = nuevo_nick.substr(0, p_n);

    if (findClientByNick(nuevo_nick)) {
        sendResponse(client, "ERROR: Nickname already in use.\n");
    } else {
        client->setNickname(nuevo_nick);
        sendResponse(client, "NICK: Your nickname is now " + nuevo_nick + "\n");
    }
}


void Server::sendWelcome(Client* client) {
    sendResponse(client, ":ircserv 001 " + client->getNickname() + " :Welcome to the 42 IRC Network!\n");
    sendResponse(client, ":ircserv 375 " + client->getNickname() + " :- 42 IRC Message of the Day -\n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " :  _  _  ____   \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : | || ||___ \\  \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : | || |_ __) | \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : |__   _/ __/  \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " :    |_||_____| \n");
    sendResponse(client, ":ircserv 376 " + client->getNickname() + " :End of /MOTD command.\n");
}




void Server::handleUser(Client* client, std::string message) {
    (void)message;
    if (!client->isAuthenticated() || client->getNickname().empty()) {
        sendResponse(client, "ERROR: Send PASS and NICK first.\n");
    } else if (client->isRegistered()) {
        sendResponse(client, ":ircserv 462 " + client->getNickname() + " :Unauthorized command (already registered)\n");
    } else {
        client->setRegistered(true);
        //sendResponse(client, ":ircserv 001 " + client->getNickname() + " :Welcome to the Network!\n");
        sendWelcome(client);
    }
    
    
}

// --- COMANDOS OPERATIVOS ---



void Server::handleJoin(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, cName, providedKey;
    
    ss >> cmd >> cName >> providedKey;

    // 1. Validación de parámetros básicos
    if (cName.empty()) {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " JOIN :Not enough parameters\n");
        return;
    }

    // 2. Comprobar existencia y si ya es miembro (Anti-duplicados)
    bool exists = (_channels.find(cName) != _channels.end());
    
    if (exists && _channels[cName]->isMember(client)) {
        // Ya está dentro, no hacemos nada para evitar el broadcast repetido
        return; 
    }

    // 3. Lógica de creación o filtros de entrada
    if (!exists) {
        // ESCENARIO A: Canal Nuevo (Entrada libre como Admin)
        _channels[cName] = new Channel(cName);
        _channels[cName]->setAdmin(client);
    } 
    else {
        // ESCENARIO B: Canal Existente (Aplicar filtros de MODOS)
        Channel* chan = _channels[cName];

        // Filtro Invite Only (+i)
        if (chan->isInviteOnly() && !chan->isInvited(client->getNickname())) {
            sendResponse(client, ":ircserv 473 " + client->getNickname() + " " + cName + " :Cannot join channel (+i)\n");
            return;
        }

        // Filtro Límite de usuarios (+l)
        if (chan->isFull()) {
            sendResponse(client, ":ircserv 471 " + client->getNickname() + " " + cName + " :Cannot join channel (+l)\n");
            return;
        }

        // Filtro Contraseña (+k)
        if (!chan->getKey().empty() && providedKey != chan->getKey()) {
            sendResponse(client, ":ircserv 475 " + client->getNickname() + " " + cName + " :Cannot join channel (+k)\n");
            return;
        }
    }

    // 4. Proceso de unión exitosa
    Channel* chan = _channels[cName];
    chan->addMember(client);
    
    // Si entró por invitación, la limpiamos
    if (chan->isInvited(client->getNickname())) {
        chan->removeGuest(client->getNickname());
    }

    // 5. Notificaciones y protocolo de bienvenida al canal
    // Anunciamos a todos (incluido el nuevo) que alguien ha entrado
    chan->broadcast(":" + client->getNickname() + " JOIN " + cName + "\n", NULL);
    
    // Mandamos el TOPIC si existe (RPL_TOPIC 332)
    if (!chan->getTopic().empty())
        sendResponse(client, ":ircserv 332 " + client->getNickname() + " " + cName + " :" + chan->getTopic() + "\n");

    // Mandamos la lista de usuarios (RPL_NAMREPLY 353)
    sendResponse(client, ":ircserv 353 " + client->getNickname() + " = " + cName + " :" + chan->getNamesList() + "\n");
    
    // Fin de la lista (RPL_ENDOFNAMES 366)
    sendResponse(client, ":ircserv 366 " + client->getNickname() + " " + cName + " :End of /NAMES list\n");
}


void Server::handlePrivmsg(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, target, content;

    ss >> cmd >> target;
    std::getline(ss, content); // Captura todo lo que sigue al target

    if (target.empty()) {
        sendResponse(client, ":ircserv 411 " + client->getNickname() + " :No recipient given (PRIVMSG)\n");
        return;
    }
    if (content.empty()) {
        sendResponse(client, ":ircserv 412 " + client->getNickname() + " :No text to send\n");
        return;
    }

    // 1. Mensaje a un CANAL
    if (target[0] == '#') {
        if (_channels.find(target) == _channels.end()) {
            sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + target + " :No such nick/channel\n");
            return;
        }

        Channel* chan = _channels[target];
        // Opcional: ¿Debe estar el usuario dentro para hablar? (Muchos servidores lo exigen)
        if (!chan->isMember(client)) {
            sendResponse(client, ":ircserv 404 " + client->getNickname() + " " + target + " :Cannot send to channel\n");
            return;
        }

        // El 'exclude' es el cliente que envía el mensaje (no quiere recibir su propio eco)
        chan->broadcast(":" + client->getNickname() + " PRIVMSG " + target + content + "\n", client);
    } 
    // 2. Mensaje PRIVADO a otro usuario
    else {
        Client* targetClient = findClientByNick(target);
        if (targetClient) {
            sendResponse(targetClient, ":" + client->getNickname() + " PRIVMSG " + target + content + "\n");
        } else {
            sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + target + " :No such nick/channel\n");
        }
    }
    checkBotCommands(client, target, content);
}

void Server::handleInvite(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, tNick, cName;
    
    if (!(ss >> cmd >> tNick >> cName)) {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " INVITE :Not enough parameters\n");
        return;
    }

    // 1. Verificar si el canal existe
    if (_channels.find(cName) == _channels.end()) {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel *chan = _channels[cName];

    // 2. [OPCIONAL 42] Verificar si el que invita está en el canal
    // (Muchos servidores IRC solo permiten invitar si tú estás dentro)
    bool isMember = false;
    const std::vector<Client*>& members = chan->getMembers();
    for (size_t i = 0; i < members.size(); ++i) {
        if (members[i] == client) { isMember = true; break; }
    }
    if (!isMember) {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cName + " :You're not on that channel\n");
        return;
    }

    // 3. Buscar al cliente invitado
    Client* target = findClientByNick(tNick);
    if (!target) {
        sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + tNick + " :No such nick\n");
        return;
    }

    // 4. --- LA PARTE CRÍTICA ---
    // Registramos al invitado en la "lista blanca" del canal
    chan->addGuest(tNick); 

    // 5. Notificar a las partes
    // Al invitado:
    std::string inviteMsg = ":" + client->getNickname() + " INVITE " + tNick + " :" + cName + "\n";
    sendResponse(target, inviteMsg);
    
    // Al emisor (RPL_INVITING 341):
    sendResponse(client, ":ircserv 341 " + client->getNickname() + " " + tNick + " " + cName + "\n");
}

void Server::handlePart(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, cName, reason;
    
    // Parseo: PART #canal :razon (opcional)
    ss >> cmd >> cName;
    std::getline(ss, reason); // Captura el resto (ej: " :Me voy a dormir")

    // 1. ¿El canal existe? (Error 403)
    if (_channels.find(cName) == _channels.end()) {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }

    Channel* chan = _channels[cName];

    // 2. ¿El usuario está realmente en ese canal? (Error 442)
    if (!chan->isMember(client)) {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cName + " :You're not on that channel\n");
        return;
    }

    // 3. Notificar a todos que el cliente se va
    // Si no hay razón, enviamos un mensaje simple. Si la hay, la incluimos.
    if (reason.empty()) reason = " :Leaving";
    chan->broadcast(":" + client->getNickname() + " PART " + cName + reason + "\n", NULL);

    // 4. Salida física
    chan->removeMember(client);

    // 5. Gestión de huérfanos (Si el canal se queda vacío, se borra)
    if (chan->getMembers().empty()) {
        delete chan;
        _channels.erase(cName);
    }
}

void Server::handleTopic(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, cn, nt;
    
    ss >> cmd >> cn;
    std::getline(ss, nt); // nt ahora tiene el resto de la línea

    // 1. ¿Existe el canal? (Error 403)
    if (_channels.find(cn) == _channels.end()) {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cn + " :No such channel\n");
        return;
    }

    Channel* chan = _channels[cn];

    // 2. ¿Está el usuario en el canal? (Error 442)
    if (!chan->isMember(client)) {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cn + " :You're not on that channel\n");
        return;
    }

    // 3. Limpieza del string nt (quitando espacios y el ':' inicial)
    // El protocolo suele enviar " :Topic nuevo"
    size_t first_not_space = nt.find_first_not_of(" ");
    if (first_not_space != std::string::npos)
        nt = nt.substr(first_not_space);
    if (!nt.empty() && nt[0] == ':')
        nt = nt.substr(1);

    // 4. CONSULTA: Si nt está vacío tras la limpieza, solo mostramos el topic
    if (nt.empty()) {
        if (chan->getTopic().empty())
            sendResponse(client, ":ircserv 331 " + client->getNickname() + " " + cn + " :No topic is set\n");
        else
            sendResponse(client, ":ircserv 332 " + client->getNickname() + " " + cn + " :" + chan->getTopic() + "\n");
        return;
    }

    // 5. CAMBIO: Filtro de seguridad +t
    if (chan->isTopicProtected() && chan->getAdmin() != client) {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cn + " :You're not channel operator\n");
        return;
    }

    // 6. Éxito: Cambiamos y avisamos a todos
    chan->setTopic(nt);
    chan->broadcast(":" + client->getNickname() + " TOPIC " + cn + " :" + nt + "\n", NULL);
}

void Server::handleKick(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, cName, targetNick, reason;
    
    // Parseo: KICK #canal usuario :razon (opcional)
    ss >> cmd >> cName >> targetNick;
    std::getline(ss, reason); // Captura el resto del mensaje como la razón
    if (reason.empty()) reason = " :Kicked by operator";

    // 1. ¿Existe el canal? (Error 403)
    if (_channels.find(cName) == _channels.end()) {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel* chan = _channels[cName];

    // 2. ¿El que ejecuta es el Admin/Operador? (Error 482)
    if (chan->getAdmin() != client) {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cName + " :You're not channel operator\n");
        return;
    }

    // 3. ¿Existe el usuario objetivo en el servidor?
    Client* target = findClientByNick(targetNick);
    
    // 4. ¿Está ese usuario realmente en el canal? (Error 441)
    if (!target || !chan->isMember(target)) {
        sendResponse(client, ":ircserv 441 " + client->getNickname() + " " + targetNick + " " + cName + " :They aren't on that channel\n");
        return;
    }

    // 5. Todo OK: Notificar la expulsión y ejecutarla
    // El mensaje de KICK se envía a todos en el canal ANTES de borrar al miembro
    chan->broadcast(":" + client->getNickname() + " KICK " + cName + " " + targetNick + " " + reason + "\n", NULL);
    
    chan->removeMember(target);
}

void Server::handleList(Client* client, std::string message) {
    (void)message; // LIST normalmente no requiere parámetros según el Subject
    
    // 1. Enviar cabecera (Opcional en algunos clientes, pero útil)
    // El protocolo IRC estándar a veces usa el 321, pero podemos ir directos al 322.

    std::map<std::string, Channel*>::iterator it;
    for (it = _channels.begin(); it != _channels.end(); ++it) {
        std::string cName = it->first;
        Channel* chan = it->second;
        
        // Calculamos el número de miembros para el mensaje
        std::stringstream ss;
        ss << chan->getMembers().size();
        std::string numUsers = ss.str();

        // 2. Enviar RPL_LIST (322)
        // Formato: :ircserv 322 <nick> <#canal> <usuarios> :<topic>
        std::string response = ":ircserv 322 " + client->getNickname() + " " + cName + " " + numUsers + " :" + chan->getTopic() + "\n";
        sendResponse(client, response);
    }

    // 3. Enviar RPL_LISTEND (323)
    // Formato: :ircserv 323 <nick> :End of /LIST
    sendResponse(client, ":ircserv 323 " + client->getNickname() + " :End of /LIST\n");
}

void Server::handleQuit(Client* client, std::string message, size_t &i) {
    (void)message;
    
    // 1. Enviamos un mensaje de error/cierre. 
    // En IRC, el servidor suele despedirse con "ERROR :Closing Link"
    std::string quitMsg = "ERROR :Closing Link\n";
    send(client->getFd(), quitMsg.c_str(), quitMsg.length(), 0);

    // 2. Ahora sí, desconectamos físicamente
    disconnectClient(i);
}

void Server::handleMode(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, cName, mode, param;
    
    // Parseo inicial
    if (!(ss >> cmd >> cName >> mode)) {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE :Not enough parameters\n");
        return;
    }

    if (_channels.find(cName) == _channels.end()) {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel* chan = _channels[cName];

    // Solo el operador/admin actual puede cambiar modos
    if (client != chan->getAdmin()) {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cName + " :You're not channel operator\n");
        return;
    }

    // --- LÓGICA DE MODOS (i, t, l, k, o) ---
    if (mode == "+i") {
        chan->setInviteOnly(true);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +i\n", NULL);
    } 
    else if (mode == "-i") {
        chan->setInviteOnly(false);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -i\n", NULL);
    }
    else if (mode == "+t") {
        chan->setTopicProtected(true);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +t\n", NULL);
    }
    else if (mode == "-t") {
        chan->setTopicProtected(false);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -t\n", NULL);
    }
    else if (mode == "+l") {
        if (ss >> param) {
            int limit = std::atoi(param.c_str());
            if (limit > 0) {
                chan->setLimit(limit);
                chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +l " + param + "\n", NULL);
            }
        } else {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +l :Not enough parameters\n");
        }
    }
    else if (mode == "-l") {
        chan->setLimit(-1);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -l\n", NULL);
    }
    else if (mode == "+k") {
        if (ss >> param) {
            chan->setKey(param);
            chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +k " + param + "\n", NULL);
        } else {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +k :Not enough parameters\n");
        }
    }
    else if (mode == "-k") {
        chan->setKey("");
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -k\n", NULL);
    }
    // --- NUEVO: MODO OPERATOR (+o) ---
    else if (mode == "+o") {
        if (ss >> param) {
            Client* target = findClientByNick(param);
            if (!target) {
                // Error 401: El nick no existe en el servidor
                sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + param + " :No such nick\n");
            } 
            else if (!chan->isMember(target)) {
                // Error 441: El usuario existe pero no está en este canal
                sendResponse(client, ":ircserv 441 " + client->getNickname() + " " + param + " " + cName + " :They aren't on that channel\n");
            } 
            else {
                // Éxito: Transferimos el poder de admin
                chan->setAdmin(target);
                chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +o " + param + "\n", NULL);
            }
        } else {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +o :Not enough parameters\n");
        }
    }
    else {
        // Modo desconocido (Error 472)
        sendResponse(client, ":ircserv 472 " + client->getNickname() + " " + mode + " :is unknown mode char to me\n");
    }
}

void Server::handleNotice(Client* client, std::string message) {
    std::stringstream ss(message);
    std::string cmd, target, content;

    // 1. Parseo: NOTICE <target> :<mensaje>
    ss >> cmd >> target;
    std::getline(ss, content);

    // En NOTICE, si falta el target o el texto, el protocolo dice SILENCIO.
    if (target.empty() || content.empty())
        return;

    // 2. ¿Es un mensaje a un CANAL?
    if (target[0] == '#') {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it != _channels.end()) {
            Channel* chan = it->second;
            // Solo difundimos si el emisor es miembro del canal (evita spam externo)
            if (chan->isMember(client)) {
                // El segundo parámetro 'client' evita que el emisor reciba su propio mensaje
                chan->broadcast(":" + client->getNickname() + " NOTICE " + target + content + "\n", client);
            }
        }
    } 
    // 3. ¿Es un mensaje PRIVADO a un usuario?
    else {
        Client* targetClient = findClientByNick(target);
        if (targetClient) {
            sendResponse(targetClient, ":" + client->getNickname() + " NOTICE " + target + content + "\n");
        }
        // Si el usuario no existe, no enviamos error 401. Simplemente ignoramos.
    }
}

void Server::checkBotCommands(Client* client, std::string target, std::string message) {
    // Limpiamos el mensaje (quitando el ':' inicial si existe)
    if (!message.empty() && message[0] == ' ') message = message.substr(1);
    if (!message.empty() && message[0] == ':') message = message.substr(1);

    std::string response = "";

    // Lógica de comandos del BOT
    if (message == "!hora") {
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string s_dt(dt);
        response = "🤖 [BOT] La hora actual es: " + s_dt.substr(0, s_dt.length() - 1);
    }
    else if (message == "!ping") {
        response = "🤖 [BOT] PONG! (Latencia: 0.0001ms... bueno, soy un bot interno, ¡vuelo!)";
    }
    else if (message == "!info") {
        response = "🤖 [BOT] Servidor IRC de 42. Usuarios conectados: " + static_cast<std::ostringstream*>( &(std::ostringstream() << _clients.size()) )->str();
    }
    else if (message == "!help") {
        response = "🤖 [BOT] Comandos disponibles: !hora, !ping, !info, !help";
    }

    // Si hay respuesta, la enviamos por NOTICE (regla de oro de los bots)
    if (!response.empty()) {
        if (target[0] == '#') {
            // Si es un canal, el bot responde al canal
            _channels[target]->broadcast(":IRC_BOT NOTICE " + target + " :" + response + "\n", NULL);
        } else {
            // Si es privado, el bot responde al usuario
            sendResponse(client, ":IRC_BOT NOTICE " + client->getNickname() + " :" + response + "\n");
        }
    }
}

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
