#include "Client.hpp"

/**
 * @note Constructor
 * Initializes the client with its file descriptor.
 * Handshake flags (_isAuthenticated, _isRegistered) are set to false by default.
 */
Client::Client(int fd) : _fd(fd), _isAuthenticated(false), _isRegistered(false) {}

Client::~Client() {}

// --- Basic Accessors ---
int         Client::getFd() const { return _fd; }

std::string Client::getNickname() const { return _nickname; }

void        Client::setNickname(const std::string& nick) { _nickname = nick; }

// --- Registration State Machine ---
bool        Client::isAuthenticated() const { return _isAuthenticated; }

void        Client::setAuthenticated(bool status) { _isAuthenticated = status; }

bool        Client::isRegistered() const { return _isRegistered; }

void        Client::setRegistered(bool status) { _isRegistered = status; }

// --- Socket Buffer Management ---

/**
 * @note Accumulates data received from the socket.
 * Essential for non-blocking I/O to handle fragmented IRC messages.
 */
void        Client::addToBuffer(std::string str) { _buffer += str; }

std::string Client::getBuffer() const { return _buffer; }

/**
 * @note Clears or updates the buffer after a command (\r\n) has been processed.
 */
void        Client::setBuffer(std::string str) { _buffer = str; }