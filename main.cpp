#include "Server.hpp"
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <limits>
#include <cctype> // isdigit e isspace

// Global variable to allow the signal handler to stop the main loop
bool server_stop = false;

// Signal handler for Ctrl+C (SIGINT)
void signalHandler(int signum) 
{
    (void)signum;
    std::cout << "\n[!] Deteniendo el servidor de forma segura..." << std::endl;
    server_stop = true;
}

/**
 * Helper function to validate if a string represents a positive number
 */
bool isValidPort(const std::string& str) 
{
    if (str.empty()) return false;
    for (size_t i = 0; i < str.length(); i++) 
    {
        if (!isdigit(str[i])) return false;
    }
    return true;
}

/**
 * Helper function to validate that the password is not only spaces
 */
bool isOnlySpaces(const std::string& str) 
{
    if (str.empty()) return true;
    for (size_t i = 0; i < str.length(); i++) 
    {
        if (!isspace(str[i])) return false;
    }
    return true;
}

int main(int argc, char **argv) 
{
    // 1. Validate number of arguments
    if (argc != 3) {
        std::cerr << "Use: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    // 2. Validate port (digits only)
    std::string portStr = argv[1];
    if (!isValidPort(portStr)) 
    {
        std::cerr << "Error: Port '" << portStr << "' is not a valid number." << std::endl;
        return 1;
    }

    // 3. Validate port range (1024 - 65535)
    long portCheck = atol(argv[1]);
    if (portCheck < 1024 || portCheck > 65535) 
    {
        std::cerr << "Error: Port out of range. Use a value between 1024 and 65535." << std::endl;
        return 1;
    }
    int port = static_cast<int>(portCheck);

    // 4. Validate password (must not be empty or contain only spaces)
    std::string password = argv[2];
    if (isOnlySpaces(password)) 
    {
        std::cerr << "Error: Password cannot be empty or consist only of spaces." << std::endl;
        return 1;
    }

    // Registramos la señal
    signal(SIGINT, signalHandler);

    try 
    {
        Server irc(port, password);
        irc.init();

        std::cout << "--- 42 IRC SERVER STARTED ---" << std::endl;
        std::cout << "Port: " << port << " | Password: [" << password << "]" << std::endl;
        std::cout << "Press Ctrl+C to exit cleanly." << std::endl;

        // Main loop controlled by the signal
        while (!server_stop) {
            irc.run(); 
        }

    } catch (const std::exception &e) 
    {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server closed." << std::endl;
    return 0;
}