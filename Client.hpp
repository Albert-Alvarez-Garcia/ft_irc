#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

/**
 * @class Client
 * @brief Represents a connected user and manages their session state.
 */
class Client {
private:
    int         _fd;              // File Descriptor for the client socket
    std::string _nickname;        // Unique identifier on the server
    std::string _buffer;          // Stores incomplete data from the socket
    
    /** @note State Flags
     * _isAuthenticated: true after PASS command is validated.
     * _isRegistered: true after NICK and USER commands are completed.
     */
    bool        _isAuthenticated;
    bool        _isRegistered;

public:
    Client(int fd);
    ~Client();

    // --- Getters & Setters ---
    int         getFd() const;
    std::string getNickname() const;
    void        setNickname(const std::string& nick);
    
    bool        isAuthenticated() const;
    void        setAuthenticated(bool status);
    
    bool        isRegistered() const;
    void        setRegistered(bool status);

    // --- Buffer Management ---
    /** * @note Crucial for Non-blocking I/O
     * Buffers data until a complete IRC message (\r\n) is received. 
     */
    void        addToBuffer(std::string str);
    std::string getBuffer() const;
    void        setBuffer(std::string str);
};

#endif