#ifndef LISTENINGSOCKET_HPP
#define LISTENINGSOCKET_HPP

#include <string>

/*
    ListeningSocket
    ---------------
    Represents a server socket bound to host:port.

    Purpose:
    - Accept incoming client connections.
    - Associated with a ServerConfig.
*/

class ListeningSocket {
public:
    ListeningSocket();
    ListeningSocket(const std::string& host, const std::string& port, int backlog);
    ListeningSocket(const ListeningSocket& other);
    ListeningSocket& operator=(const ListeningSocket& other);
    ~ListeningSocket();

    bool open(const std::string& host, const std::string& port, int backlog);
    int acceptClient() const;
    bool addToEpoll(int epfd) const;

    int getFd() const;
    std::string getHost() const;
    std::string getPort() const;

private:
    int fd;
    std::string host_;
    std::string port_;
};

#endif
