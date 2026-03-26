#include "Channel.hpp"
#include <sys/socket.h>

// Constructor actualizado: inicializamos _admin a NULL e _inviteOnly a false
Channel::Channel(std::string name) 
    : _name(name), _topic("No topic is set"), _admin(NULL), _inviteOnly(false), _topicProtected(true) {}

Channel::~Channel() {}

std::string Channel::getName() const { return _name; }

void Channel::addMember(Client* client) {
    // 1. Verificamos si el puntero del cliente ya existe en el vector
    for (size_t i = 0; i < _members.size(); i++) {
        if (_members[i] == client) {
            return; // Ya es miembro, no hacemos nada y salimos
        }
    }

    // 2. Si llegamos aquí, es que no estaba. Lo añadimos.
    _members.push_back(client);

    // 3. Asignamos admin si es el primero (esta lógica sigue igual)
    if (_members.size() == 1) {
        _admin = client;
    }
}

const std::vector<Client*>& Channel::getMembers() const { return _members; }

void Channel::removeMember(Client* client) {
    for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it) {
        if (*it == client) {
            _members.erase(it);
            
            // LÓGICA DE SUCESIÓN:
            // Si el que se va era el admin y el canal NO se ha quedado vacío...
            if (client == _admin && !_members.empty()) {
                _admin = _members[0]; // El siguiente de la lista toma el mando
                
                // Notificamos a todos del cambio de mando
                std::string notice = ":ircserv NOTICE " + _name + " :El usuario " + _admin->getNickname() + " es ahora el operador del canal.\n";
                broadcast(notice, NULL);
            } else if (_members.empty()) {
                _admin = NULL;
            }
            break;
        }
    }
}

// --- FUNCIÓN PARA EL COMANDO NAMES ---
std::string Channel::getNamesList() {
    std::string list = "";
    for (size_t i = 0; i < _members.size(); i++) {
        if (_members[i] == _admin)
            list += "@";
        list += _members[i]->getNickname();
        
        if (i < _members.size() - 1)
            list += " ";
    }
    return list;
}

void Channel::broadcast(std::string message, Client* exclude) {
    for (size_t i = 0; i < _members.size(); i++) {
        if (_members[i] != exclude) {
            send(_members[i]->getFd(), message.c_str(), message.length(), 0);
        }
    }
}

void Channel::setTopic(std::string newTopic) { _topic = newTopic; }
std::string Channel::getTopic() const { return _topic; }

void Channel::setAdmin(Client* client) { _admin = client; }
Client* Channel::getAdmin() const { return _admin; }

// --- NUEVOS MÉTODOS PARA GESTIÓN DE INVITACIONES (+i) ---

bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

void Channel::setInviteOnly(bool status) {
    _inviteOnly = status;
}

bool Channel::isInvited(std::string nick) const {
    for (size_t i = 0; i < _invitedNicks.size(); ++i) {
        if (_invitedNicks[i] == nick)
            return true;
    }
    return false;
}

void Channel::addGuest(std::string nick) {
    if (!isInvited(nick)) {
        _invitedNicks.push_back(nick);
    }
}

void Channel::removeGuest(std::string nick) {
    for (std::vector<std::string>::iterator it = _invitedNicks.begin(); it != _invitedNicks.end(); ++it) {
        if (*it == nick) {
            _invitedNicks.erase(it);
            break;
        }
    }
}

bool Channel::isMember(Client* cl) const {
    for (size_t i = 0; i < _members.size(); i++) {
        if (_members[i] == cl)
            return true;
    }
    return false;
}

void Channel::setTopicProtected(bool status) { _topicProtected = status; }
bool Channel::isTopicProtected() const { return _topicProtected; }
