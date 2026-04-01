#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>



// --- GESTIÓN DE RED ---





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





