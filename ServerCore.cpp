
#include "Server.hpp"

//--CONSTRUCTOR--

Server::Server(int port, std::string password) : _port(port), _password(password), _serverSocket(-1) {
    std::cout << "Servidor creado para el puerto " << _port << std::endl;
}

Server::~Server() 
{
    if (_serverSocket != -1) close(_serverSocket);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
}

// --- INITIALIZATION AND MAIN LOOP ---

void Server::init() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket == -1) 
        throw std::runtime_error("Error: Failed to create socket");

    int en = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) == -1) 
        throw std::runtime_error("Error: setsockopt (SO_REUSEADDR) failed");

    if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) == -1) 
        throw std::runtime_error("Error: fcntl (O_NONBLOCK) failed");

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_serverSocket, (struct sockaddr *)&address, sizeof(address)) == -1) 
        throw std::runtime_error("Error: bind failed (check if port is already in use)");

    if (listen(_serverSocket, SOMAXCONN) == -1) 
        throw std::runtime_error("Error: listen failed");

    std::cout << "[LOG] Server listening on port " << _port << "..."  << std::endl;
}

void Server::run() 
{
    struct pollfd v_server_fd;
    v_server_fd.fd = _serverSocket;
    v_server_fd.events = POLLIN;
    v_server_fd.revents = 0;
    _fds.push_back(v_server_fd);

/* Removed this poll check to comply with the project's requirement 
   of a 'single poll', despite it being a safety measure in case 
   the loop is not entered. */

    /*
    int poll_count = poll(&_fds[0], _fds.size(), 1000); 
    
    if (poll_count < 0) {
        // Si poll falla por una señal (Ctrl+C), no es un error real
        return;
    }
*/
    while (true) 
    {
        // Use &_fds[0] to pass the pointer to the internal array
        if (poll(&_fds[0], _fds.size(), -1) == -1) 
        {
            // Prevent exception throwing if the server closes due to a signal
            break; 
        }

        for (size_t i = 0; i < _fds.size(); i++) 
        {
            if (_fds[i].revents & POLLIN) 
            {
                if (_fds[i].fd == _serverSocket) 
                {
                    acceptNewConnection();
                } 
                else 
                {
                    // Store current file descriptor for later comparison
                    int fd_antes = _fds[i].fd;
                    
                    handleClientData(i);

                    // --- CRITICAL BUGFIX ---
                    // Handle cases where handleClientData removes a client, causing the _fds vector
                    // to resize and shift. Verify if the current index 'i' still points to the same FD.
                    if (i < _fds.size() && _fds[i].fd != fd_antes) 
                    {
                        // Adjust index to prevent skipping the next element
                        i--; 
                    } else if (i >= _fds.size()) 
                    {
                        // Exit the loop if the last element was removed
                        break;
                    }
                }
            }
        }
    }
}




