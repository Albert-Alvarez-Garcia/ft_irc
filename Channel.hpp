#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include "Client.hpp"

/**
 * @class Channel
 * @brief Manages IRC channel state, member lists, and mode restrictions.
 */
class Channel {
private:
    // --- Core Data ---
    std::string             _name;          // Channel name (must start with #)
    std::vector<Client*>    _members;       // List of currently connected clients
    std::string             _topic;         // Current channel topic
    Client* _admin;         // Channel Operator (ChanOp)

    // --- Mode +i: Invite-Only ---
    /** @note If true, only invited users can JOIN via the INVITE command. */
    bool                     _inviteOnly;      
    std::vector<std::string> _invitedNicks;    

    // --- Mode +t: Topic Protection ---
    /** @note If true, only the Admin/Operator can change the channel topic. */
    bool                     _topicProtected;

    // --- Mode +l: User Limit ---
    /** @note Max number of members allowed. Value -1 means no limit. */
    int                      _limit; 

    // --- Mode +k: Channel Key (Password) ---
    /** @note Password required to join. Empty string if no key is set. */
    std::string              _key;

public:
    Channel(std::string name);
    ~Channel();

    // --- Basic Getters & Setters ---
    std::string getName() const;
    const std::vector<Client*>& getMembers() const;
    void        addMember(Client* client);
    void        removeMember(Client* client);
    void        setAdmin(Client* client);
    Client* getAdmin() const;

    // --- Communication ---
    /** @note Sends a message to all members, optionally excluding the sender. */
    void        broadcast(std::string message, Client* exclude);
    /** @note Returns a string of all nicknames for the RPL_NAMREPLY (353) numeric. */
    std::string getNamesList();

    // --- Topic Management (+t logic) ---
    void        setTopic(std::string newTopic);
    std::string getTopic() const;
    void        setTopicProtected(bool status);
    bool        isTopicProtected() const;

    // --- Invitation Management (+i logic) ---
    void        setInviteOnly(bool status);
    bool        isInviteOnly() const;
    void        addGuest(std::string nick);
    bool        isInvited(std::string nick) const;
    void        removeGuest(std::string nick);

    // --- Limit Management (+l logic) ---
    void        setLimit(int limit) { _limit = limit; }
    int         getLimit() const { return _limit; }
    /** @note Returns true if the current member count reaches the defined limit. */
    bool        isFull() const {
        if (_limit > 0 && (int)_members.size() >= _limit) return true;
        return false;
    }

    // --- Key Management (+k logic) ---
    void        setKey(std::string key) { _key = key; }
    std::string getKey() const { return _key; }

    // --- Utility ---
    bool        isMember(Client* cl) const;
};

#endif