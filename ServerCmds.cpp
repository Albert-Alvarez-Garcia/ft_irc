#include "Server.hpp"

void Server::handleJoin(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, cName, providedKey;
    
    ss >> cmd >> cName >> providedKey;

    // 1. Basic parameter validation
    if (cName.empty()) 
    {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " JOIN :Not enough parameters\n");
        return;
    }

    // 2. Check existence and membership (Anti-duplicate)
    bool exists = (_channels.find(cName) != _channels.end());
    
    if (exists && _channels[cName]->isMember(client)) 
    {
        // Already a member, do nothing to avoid redundant broadcasts
        return; 
    }

    // 3. Creation logic or entry filters
    if (!exists) 
    {
        // SCENARIO A: New Channel (User joins as Admin)
        _channels[cName] = new Channel(cName);
        _channels[cName]->setAdmin(client);
    } 
    else 
    {
        // SCENARIO B: Existing Channel (Apply MODE filters)
        Channel* chan = _channels[cName];

        // Invite Only Filter (+i)
        if (chan->isInviteOnly() && !chan->isInvited(client->getNickname())) 
        {
            sendResponse(client, ":ircserv 473 " + client->getNickname() + " " + cName + " :Cannot join channel (+i)\n");
            return;
        }

        // User Limit Filter (+l)
        if (chan->isFull()) 
        {
            sendResponse(client, ":ircserv 471 " + client->getNickname() + " " + cName + " :Cannot join channel (+l)\n");
            return;
        }

        // Password Filter (+k)
        if (!chan->getKey().empty() && providedKey != chan->getKey()) 
        {
            sendResponse(client, ":ircserv 475 " + client->getNickname() + " " + cName + " :Cannot join channel (+k)\n");
            return;
        }
    }

    // 4. Successful Join Process
    Channel* chan = _channels[cName];
    chan->addMember(client);
    
    // Clear invitation if user was invited
    if (chan->isInvited(client->getNickname())) 
    {
        chan->removeGuest(client->getNickname());
    }

    // 5. Notifications and channel welcome protocol
    // Announce to everyone (including the new member) that someone joined
    chan->broadcast(":" + client->getNickname() + " JOIN " + cName + "\n", NULL);
    
    // Send TOPIC if it exists (RPL_TOPIC 332)
    if (!chan->getTopic().empty())
        sendResponse(client, ":ircserv 332 " + client->getNickname() + " " + cName + " :" + chan->getTopic() + "\n");

    // Send member list (RPL_NAMREPLY 353)
    sendResponse(client, ":ircserv 353 " + client->getNickname() + " = " + cName + " :" + chan->getNamesList() + "\n");
    
    // End of list (RPL_ENDOFNAMES 366)
    sendResponse(client, ":ircserv 366 " + client->getNickname() + " " + cName + " :End of /NAMES list\n");
}

void Server::handlePrivmsg(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, target, content;

    ss >> cmd >> target;
    std::getline(ss, content); // Capture everything after the target

    if (target.empty()) 
    {
        sendResponse(client, ":ircserv 411 " + client->getNickname() + " :No recipient given (PRIVMSG)\n");
        return;
    }
    if (content.empty()) 
    {
        sendResponse(client, ":ircserv 412 " + client->getNickname() + " :No text to send\n");
        return;
    }

    // 1. Message to a CHANNEL
    if (target[0] == '#') 
    {
        if (_channels.find(target) == _channels.end()) 
        {
            sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + target + " :No such nick/channel\n");
            return;
        }

        Channel* chan = _channels[target];
        // Check if user must be a member to speak (Standard IRC behavior)
        if (!chan->isMember(client)) 
        {
            sendResponse(client, ":ircserv 404 " + client->getNickname() + " " + target + " :Cannot send to channel\n");
            return;
        }

        // 'exclude' parameter ensures the sender doesn't receive their own echo
        chan->broadcast(":" + client->getNickname() + " PRIVMSG " + target + content + "\n", client);
    } 
    // 2. PRIVATE message to another user
    else {
        Client* targetClient = findClientByNick(target);
        if (targetClient) 
        {
            sendResponse(targetClient, ":" + client->getNickname() + " PRIVMSG " + target + content + "\n");
        } 
        else 
        {
            sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + target + " :No such nick/channel\n");
        }
    }
    checkBotCommands(client, target, content);
}

void Server::handleInvite(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, tNick, cName;
    
    if (!(ss >> cmd >> tNick >> cName)) 
    {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " INVITE :Not enough parameters\n");
        return;
    }

    // 1. Verify channel existence
    if (_channels.find(cName) == _channels.end()) 
    {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel *chan = _channels[cName];

    // 2. Verify if the inviter is in the channel
    bool isMember = false;
    const std::vector<Client*>& members = chan->getMembers();
    for (size_t i = 0; i < members.size(); ++i) 
    {
        if (members[i] == client) { isMember = true; break; }
    }
    if (!isMember) 
    {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cName + " :You're not on that channel\n");
        return;
    }

    // 3. Find the target client
    Client* target = findClientByNick(tNick);
    if (!target) 
    {
        sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + tNick + " :No such nick\n");
        return;
    }

    // 4. CRITICAL: Add target to channel's "guest list" (whitelist)
    chan->addGuest(tNick); 

    // 5. Notify both parties
    // To the invited guest:
    std::string inviteMsg = ":" + client->getNickname() + " INVITE " + tNick + " :" + cName + "\n";
    sendResponse(target, inviteMsg);
    
    // To the inviter (RPL_INVITING 341):
    sendResponse(client, ":ircserv 341 " + client->getNickname() + " " + tNick + " " + cName + "\n");
}

void Server::handlePart(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, cName, reason;
    
    // Parsing: PART #channel :reason (optional)
    ss >> cmd >> cName;
    std::getline(ss, reason); 

    // 1. Does the channel exist? (Error 403)
    if (_channels.find(cName) == _channels.end()) 
    {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }

    Channel* chan = _channels[cName];

    // 2. Is the user actually in the channel? (Error 442)
    if (!chan->isMember(client)) 
    {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cName + " :You're not on that channel\n");
        return;
    }

    // 3. Notify everyone about the departure
    if (reason.empty()) reason = " :Leaving";
    chan->broadcast(":" + client->getNickname() + " PART " + cName + reason + "\n", NULL);

    // 4. Physical removal
    chan->removeMember(client);

    // 5. Orphan management (If channel is empty, delete it)
    if (chan->getMembers().empty()) 
    {
        delete chan;
        _channels.erase(cName);
    }
}

void Server::handleTopic(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, cn, nt;
    
    ss >> cmd >> cn;
    std::getline(ss, nt); 

    // 1. Channel exists? (Error 403)
    if (_channels.find(cn) == _channels.end()) 
    {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cn + " :No such channel\n");
        return;
    }

    Channel* chan = _channels[cn];

    // 2. Is the user a member? (Error 442)
    if (!chan->isMember(client)) 
    {
        sendResponse(client, ":ircserv 442 " + client->getNickname() + " " + cn + " :You're not on that channel\n");
        return;
    }

    // 3. String cleanup (trimming spaces and initial ':')
    size_t first_not_space = nt.find_first_not_of(" ");
    if (first_not_space != std::string::npos)
        nt = nt.substr(first_not_space);
    if (!nt.empty() && nt[0] == ':')
        nt = nt.substr(1);

    // 4. QUERY: If nt is empty after cleanup, just show current topic
    if (nt.empty()) 
    {
        if (chan->getTopic().empty())
            sendResponse(client, ":ircserv 331 " + client->getNickname() + " " + cn + " :No topic is set\n");
        else
            sendResponse(client, ":ircserv 332 " + client->getNickname() + " " + cn + " :" + chan->getTopic() + "\n");
        return;
    }

    // 5. CHANGE: Protected topic filter (+t)
    if (chan->isTopicProtected() && chan->getAdmin() != client) 
    {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cn + " :You're not channel operator\n");
        return;
    }

    // 6. Success: Update and broadcast
    chan->setTopic(nt);
    chan->broadcast(":" + client->getNickname() + " TOPIC " + cn + " :" + nt + "\n", NULL);
}

void Server::handleKick(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, cName, targetNick, reason;
    
    // Parsing: KICK #channel user :reason (optional)
    ss >> cmd >> cName >> targetNick;
    std::getline(ss, reason); 
    if (reason.empty()) reason = " :Kicked by operator";

    // 1. Channel exists? (Error 403)
    if (_channels.find(cName) == _channels.end()) 
    {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel* chan = _channels[cName];

    // 2. Is the sender an Operator? (Error 482)
    if (chan->getAdmin() != client) 
    {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cName + " :You're not channel operator\n");
        return;
    }

    // 3. Find target client
    Client* target = findClientByNick(targetNick);
    
    // 4. Is the target actually in the channel? (Error 441)
    if (!target || !chan->isMember(target)) 
    {
        sendResponse(client, ":ircserv 441 " + client->getNickname() + " " + targetNick + " " + cName + " :They aren't on that channel\n");
        return;
    }

    // 5. Success: Notify and execute kick
    // KICK message is broadcast before removing the member
    chan->broadcast(":" + client->getNickname() + " KICK " + cName + " " + targetNick + " " + reason + "\n", NULL);
    
    chan->removeMember(target);
}

void Server::handleList(Client* client, std::string message) 
{
    (void)message; 
    
    std::map<std::string, Channel*>::iterator it;
    for (it = _channels.begin(); it != _channels.end(); ++it) 
    {
        std::string cName = it->first;
        Channel* chan = it->second;
        
        std::stringstream ss;
        ss << chan->getMembers().size();
        std::string numUsers = ss.str();

        // RPL_LIST (322)
        // Format: :ircserv 322 <nick> <#channel> <users> :<topic>
        std::string response = ":ircserv 322 " + client->getNickname() + " " + cName + " " + numUsers + " :" + chan->getTopic() + "\n";
        sendResponse(client, response);
    }

    // RPL_LISTEND (323)
    sendResponse(client, ":ircserv 323 " + client->getNickname() + " :End of /LIST\n");
}

void Server::handleQuit(Client* client, std::string message, size_t &i) 
{
    (void)message;
    
    // IRC standard goodbye message
    std::string quitMsg = "ERROR :Closing Link\n";
    send(client->getFd(), quitMsg.c_str(), quitMsg.length(), 0);

    // Disconnect physically
    disconnectClient(i);
}

void Server::handleMode(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, cName, mode, param;
    
    if (!(ss >> cmd >> cName >> mode)) 
    {
        sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE :Not enough parameters\n");
        return;
    }

    if (_channels.find(cName) == _channels.end()) 
    {
        sendResponse(client, ":ircserv 403 " + client->getNickname() + " " + cName + " :No such channel\n");
        return;
    }
    Channel* chan = _channels[cName];

    if (client != chan->getAdmin()) 
    {
        sendResponse(client, ":ircserv 482 " + client->getNickname() + " " + cName + " :You're not channel operator\n");
        return;
    }

    // --- MODE LOGIC (i, t, l, k, o) ---
    if (mode == "+i") 
    {
        chan->setInviteOnly(true);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +i\n", NULL);
    } 
    else if (mode == "-i") 
    {
        chan->setInviteOnly(false);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -i\n", NULL);
    }
    else if (mode == "+t") 
    {
        chan->setTopicProtected(true);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +t\n", NULL);
    }
    else if (mode == "-t") 
    {
        chan->setTopicProtected(false);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -t\n", NULL);
    }
    else if (mode == "+l") 
    {
        if (ss >> param) 
        {
            int limit = std::atoi(param.c_str());
            if (limit > 0) {
                chan->setLimit(limit);
                chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +l " + param + "\n", NULL);
            }
        } 
        else 
        {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +l :Not enough parameters\n");
        }
    }
    else if (mode == "-l") 
    {
        chan->setLimit(-1);
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -l\n", NULL);
    }
    else if (mode == "+k") 
    {
        if (ss >> param) 
        {
            chan->setKey(param);
            chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +k " + param + "\n", NULL);
        } 
        else 
        {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +k :Not enough parameters\n");
        }
    }
    else if (mode == "-k") 
    {
        chan->setKey("");
        chan->broadcast(":" + client->getNickname() + " MODE " + cName + " -k\n", NULL);
    }
    else if (mode == "+o") 
    {
        if (ss >> param) 
        {
            Client* target = findClientByNick(param);
            if (!target) 
            {
                sendResponse(client, ":ircserv 401 " + client->getNickname() + " " + param + " :No such nick\n");
            } 
            else if (!chan->isMember(target)) 
            {
                sendResponse(client, ":ircserv 441 " + client->getNickname() + " " + param + " " + cName + " :They aren't on that channel\n");
            } 
            else 
            {
                chan->setAdmin(target);
                chan->broadcast(":" + client->getNickname() + " MODE " + cName + " +o " + param + "\n", NULL);
            }
        } 
        else 
        {
            sendResponse(client, ":ircserv 461 " + client->getNickname() + " MODE +o :Not enough parameters\n");
        }
    }
    else 
    {
        sendResponse(client, ":ircserv 472 " + client->getNickname() + " " + mode + " :is unknown mode char to me\n");
    }
}

void Server::handleNotice(Client* client, std::string message) 
{
    std::stringstream ss(message);
    std::string cmd, target, content;

    ss >> cmd >> target;
    std::getline(ss, content);

    // NOTICE protocol dictates silence on missing target or text
    if (target.empty() || content.empty())
        return;

    if (target[0] == '#') 
    {
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it != _channels.end()) 
        {
            Channel* chan = it->second;
            if (chan->isMember(client)) 
            {
                chan->broadcast(":" + client->getNickname() + " NOTICE " + target + content + "\n", client);
            }
        }
    } 
    else 
    {
        Client* targetClient = findClientByNick(target);
        if (targetClient) 
        {
            sendResponse(targetClient, ":" + client->getNickname() + " NOTICE " + target + content + "\n");
        }
    }
}

void Server::checkBotCommands(Client* client, std::string target, std::string message) 
{
    if (!message.empty() && message[0] == ' ') message = message.substr(1);
    if (!message.empty() && message[0] == ':') message = message.substr(1);

    std::string response = "";

    if (message == "!time") 
    {
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string s_dt(dt);
        response = "🤖 [BOT] Current Server Time: " + s_dt.substr(0, s_dt.length() - 1);
    }
    else if (message == "!ping") 
    {
        response = "🤖 [BOT] PONG! (Latency: 0.0001ms... well, I'm an internal bot, I fly!)";
    }
    else if (message == "!info") 
    {
        std::stringstream ss;
        ss << _clients.size();
        response = "🤖 [BOT] 42 IRC Server. Connected Users: " + ss.str();
    }
    else if (message == "!help") 
    {
        response = "🤖 [BOT] Available commands: !time, !ping, !info, !help";
    }

    if (!response.empty()) 
    {
        if (target[0] == '#') 
        {
            _channels[target]->broadcast(":IRC_BOT NOTICE " + target + " :" + response + "\n", NULL);
        } 
        else 
        {
            sendResponse(client, ":IRC_BOT NOTICE " + client->getNickname() + " :" + response + "\n");
        }
    }
}
