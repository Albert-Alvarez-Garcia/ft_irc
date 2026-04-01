#include "Server.hpp"

void Server::handlePass(Client* client, std::string message) 
{
    if (message.length() < 6) return;
    if (message.substr(5) == _password) 
    {
        client->setAuthenticated(true);
        sendResponse(client, "PASS: Password accepted.\n");
    } else {
        sendResponse(client, "PASS: Wrong password.\n");
    }
}

void Server::handleNick(Client* client, std::string message) 
{
    if (!client->isAuthenticated()) 
    {
        sendResponse(client, "ERROR: Send PASS first.\n");
        return;
    }
    std::string new_nick = message.substr(5);
    size_t p_n = new_nick.find_first_of(" \r\n");
    if (p_n != std::string::npos) new_nick = new_nick.substr(0, p_n);

    if (findClientByNick(new_nick)) 
    {
        sendResponse(client, "ERROR: Nickname already in use.\n");
    } 
    else 
    {
        client->setNickname(new_nick);
        sendResponse(client, "NICK: Your nickname is now " + new_nick + "\n");
    }
}

void Server::handleUser(Client* client, std::string message) 
{
    (void)message;
    if (!client->isAuthenticated() || client->getNickname().empty()) 
    {
        sendResponse(client, "ERROR: Send PASS and NICK first.\n");
    } else if (client->isRegistered()) 
    {
        sendResponse(client, ":ircserv 462 " + client->getNickname() + " :Unauthorized command (already registered)\n");
    } 
    else 
    {
        client->setRegistered(true);
        // Registration successful, send MOTD and welcome messages
        sendWelcome(client);
    }
}

void Server::sendWelcome(Client* client) 
{
    sendResponse(client, ":ircserv 001 " + client->getNickname() + " :Welcome to the 42 IRC Network!\n");
    sendResponse(client, ":ircserv 375 " + client->getNickname() + " :- 42 IRC Message of the Day -\n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " :  _  _  ____   \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : | || ||___ \\  \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : | || |_ __) | \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " : |__   _/ __/  \n");
    sendResponse(client, ":ircserv 372 " + client->getNickname() + " :    |_||_____| \n");
    sendResponse(client, ":ircserv 376 " + client->getNickname() + " :End of /MOTD command.\n");
}




