#include "Channel.hpp"
#include <sys/socket.h>

/**
 * @note Constructor
 * Default values: No topic set, no admin, Invite-Only disabled, 
 * but Topic Protection (+t) enabled by default as per common IRC standards.
 */
Channel::Channel(std::string name) 
    : _name(name), _topic("No topic is set"), _admin(NULL), _inviteOnly(false), _topicProtected(true), _limit(-1) {}

Channel::~Channel() {}

std::string Channel::getName() const { return _name; }

void Channel::addMember(Client* client) 
{
    // 1. Safety check: ensure client pointer is not already in the list
    for (size_t i = 0; i < _members.size(); i++) 
    {
        if (_members[i] == client) 
        {
            return; 
        }
    }
    // 2. Add client to the membership vector
    _members.push_back(client);

    // 3. Admin assignment: the first user to join becomes the Channel Operator
    if (_members.size() == 1) 
    {
        _admin = client;
    }
}

const std::vector<Client*>& Channel::getMembers() const { return _members; }

void Channel::removeMember(Client* client) 
{
    for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it) 
    {
        if (*it == client) 
        {
            _members.erase(it);
            
            /** * @note ADMIN SUCCESSION LOGIC
             * If the departing user was the operator, we pass the 'crown' 
             * to the next available user in the list to prevent orphaned channels.
             */
            if (client == _admin && !_members.empty()) 
            {
                _admin = _members[0]; 
                std::string notice = ":ircserv NOTICE " + _name + " :User " + _admin->getNickname() + " is now the channel operator.\n";
                broadcast(notice, NULL);
            } else if (_members.empty()) 
            {
                _admin = NULL;
            }
            break;
        }
    }
}

/** * @note NAMES Command Support (RPL_NAMREPLY - 353)
 * Generates a space-separated list of nicks. Operators are prefixed with '@'.
 */
std::string Channel::getNamesList() 
{
    std::string list = "";
    for (size_t i = 0; i < _members.size(); i++) 
    {
        if (_members[i] == _admin)
            list += "@";
        list += _members[i]->getNickname();
        
        if (i < _members.size() - 1)
            list += " ";
    }
    return list;
}

/** * @note BROADCAST Logic
 * Sends a message to all members in the channel. 
 * 'exclude' is used to avoid echoing the message back to the sender.
 */
void Channel::broadcast(std::string message, Client* exclude) 
{
    for (size_t i = 0; i < _members.size(); i++) 
    {
        if (_members[i] != exclude) 
        {
            send(_members[i]->getFd(), message.c_str(), message.length(), 0);
        }
    }
}

void Channel::setTopic(std::string newTopic) { _topic = newTopic; }
std::string Channel::getTopic() const { return _topic; }

void Channel::setAdmin(Client* client) { _admin = client; }
Client* Channel::getAdmin() const { return _admin; }

// --- INVITE-ONLY (+i) MANAGEMENT ---

bool Channel::isInviteOnly() const { return _inviteOnly; }
void Channel::setInviteOnly(bool status) { _inviteOnly = status; }

/** @note Checks if a nickname exists in the channel's invitation whitelist. */
bool Channel::isInvited(std::string nick) const 
{
    for (size_t i = 0; i < _invitedNicks.size(); ++i) 
    {
        if (_invitedNicks[i] == nick)
            return true;
    }
    return false;
}

void Channel::addGuest(std::string nick) 
{
    if (!isInvited(nick)) 
    {
        _invitedNicks.push_back(nick);
    }
}

void Channel::removeGuest(std::string nick) 
{
    for (std::vector<std::string>::iterator it = _invitedNicks.begin(); it != _invitedNicks.end(); ++it) 
    {
        if (*it == nick) 
        {
            _invitedNicks.erase(it);
            break;
        }
    }
}

bool Channel::isMember(Client* cl) const 
{
    for (size_t i = 0; i < _members.size(); i++) 
    {
        if (_members[i] == cl)
            return true;
    }
    return false;
}

// --- TOPIC PROTECTION (+t) MANAGEMENT ---
void Channel::setTopicProtected(bool status) { _topicProtected = status; }
bool Channel::isTopicProtected() const { return _topicProtected; }