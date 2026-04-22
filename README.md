*This project has been created as part of the 42 curriculum by \<albealva\> and \<groman\>.*

# **ft\_irc \- Internet Relay Chat Server**

## **📝 Description**

The goal of this project is to create an **IRC server** in **C++ 98**. It is a multi-user communication system where clients can connect, join channels, and exchange messages in real-time. The server handles multiple simultaneous connections using non-blocking I/O and the poll() mechanism to manage file descriptors efficiently, adhering to the **RFC 2812** standard.

Key features include:

* Full client authentication (PASS, NICK, USER).  
* Channel management (JOIN, PART, KICK, INVITE, TOPIC).  
* Private and group messaging (PRIVMSG, NOTICE).  
* Channel operator privileges and modes (i, t, k, o, l).  
* Support for standard IRC clients like **Irssi**.

## **🛠️ Instructions**

### **Compilation**

The project includes a Makefile with standard rules. To compile the server, run:

make

### **Execution**

Start the server by providing a port and a password:

./ircserv \<port\> \<password\>

### **Troubleshooting: Port Unblocking**

If the server closed unexpectedly and the port remains occupied ("Address already in use"):

\# Find the process ID (PID) using port 6667  
lsof \-i :6667

\# Kill the process (replace \<PID\> with the number from the command above)  
kill \-9 \<PID\>

### **Memory Leak Check (Valgrind)**

To ensure memory safety and undefined behavior tracking, run:

valgrind \--leak-check=full \--show-leak-kinds=all ./ircserv 6667 1234

*Note: Always close the server using Ctrl+C (SIGINT) to allow Valgrind to perform the final memory sweep correctly.*

## **👥 Multi-Client Testing**

### **1\. Automated Setup (Recommended)**

We provide a script to quickly stress-test the server with 6 different users joining the same channel:

cd extras
chmod \+x open\_clients.sh  
./open\_clients.sh

*Note: This script automates the PASS, NICK, USER, and JOIN \#42 sequence for each client.*

### **2\. Manual Connection (Netcat)**

If you prefer to connect manually using nc:

nc localhost 6667

**Required sequence after connecting:**

1. PASS \<password\>  
2. NICK \<nickname\>  
3. USER \<username\> 0 \* :\<realname\>

## **⌨️ Server Commands Reference**

------------------------------------------------------------------------------------------------------------------------------
| Command               | Syntax                                    | Description                                            |
| :-------------------- | :---------------------------------------- | :----------------------------------------------------- |
| **PASS**              | PASS \<password\>                         | Authenticates the client with the server password.     |
| **NICK**              | NICK \<nickname\>                         | Sets your unique identifier on the server.             |
| **USER**              | USER \<user\> \<mode\> \* :\<real\>       | Sets your username and real name.                      |
| **JOIN**              | /JOIN \#\<channel\> \[key\]               | Joins a channel (requires key if mode \+k is set).     |
| **PRIVMSG**           | /PRIVMSG \<target\> :\<text\>             | Sends a private message to a user or a channel.        |
| **NOTICE (BONUS)**    | /NOTICE \<target\> :\<text\>              | Similar to PRIVMSG, but no automatic replies are sent. |
| **PART**              | /PART \#\<channel\> \[reason\]            | Leaves the specified channel.                          |
| **KICK**              | /KICK \#\<ch\> \<user\> \[reason\]        | **(Ops only)** Forcibly removes a user from a channel. |
| **INVITE**            | /INVITE \<user\> \#\<channel\>            | Invites a user to an Invite-only channel (+i).         |
| **TOPIC**             | /TOPIC \#\<ch\> \[:new\_topic\]           | Views or changes the channel's topic.                  |
| **QUIT**              | /QUIT \[message\]                         | Disconnects and notifies all common channels.          |
| **MODE**              | /MODE \#\<ch\> \<modestring\> \[args\]    | **(Ops only)** Manages channel/user permissions.       |
| **BOT (Bonus)**       | \!\<command\>                             | Interactive bot triggers (\!info, \!time, \!ping).     |
------------------------------------------------------------------------------------------------------------------------------
### **🛠️ Detailed MODE Parameters (MODE \#channel)**

* \+i / \-i:            |**Invite-only** channel.  
* \+t / \-t:            |**Topic** restricted to operators.  
* \+k / \-k \<key\>     | Sets or removes the channel **password**.  
* \+o / \-o \<user\>    | Gives or takes **operator** status.  
* \+l / \-l \<limit\>   | Sets or removes the maximum **user limit**.

## **🤖 Bonus Features**

### **Interactive Bot System**

Our server includes a dedicated Bot that responds to specific triggers via private message or channel:

* \!info:               |Displays information about the server version and authors.  
* \!time:               |Returns the current server time and date.  
* \!ping:               |The bot replies with "PONG\!" to check latency.

### **File Transfer (DCC)**

Our server supports **DCC (Direct Client-to-Client)** via Irssi:

* **Send:**             |/dcc send \<Receiver\_Nick\> /path/to/file.txt  
* **Receive:**          |/dcc get \<Sender\_Nick\>  
* **Status:**           |/dcc list

## **📚 Resources**

### **References**

* [RFC 2812 \- Internet Relay Chat: Client Protocol](https://tools.ietf.org/html/rfc2812)  
* [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)  
* [Modern IRC Client Protocol Documentation](https://modern.ircdocs.horse/)

### **AI Usage Disclosure**

**Gemini 2.5 Flash** was used as a peer-collaborator for the following tasks:

* **Documentation & Formatting:**   Structuring this README.md and translating technical requirements into English.  
* **Troubleshooting:**              Explaining Irssi's behavior during the NICK command handshake and debugging port conflicts.  
* **Protocol Verification:**        Validating IRC response codes and the logic for the MODE flag handling.
