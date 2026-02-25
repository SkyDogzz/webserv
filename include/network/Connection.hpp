#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>

/*
    Connection
    ----------
    Represents one client TCP connection.

    Purpose:
    - Stores socket fd.
    - Holds read/write buffers.
    - Tracks connection state.
    - Manages keep-alive behavior.

    This is the runtime state machine for each client.
*/

class Connection {
public:
    int fd;
    std::string in_buffer;
    std::string out_buffer;
    bool keep_alive;
};

#endif
