#ifndef LISTENINGSOCKET_HPP
#define LISTENINGSOCKET_HPP

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
    int fd;
};

#endif
