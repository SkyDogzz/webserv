#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <iostream>
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
    Connection();
    Connection(int fd, int listen_fd);
    ~Connection();

    void init(int fd, int listen_fd);
    int getFd() const;
    int getListenFd() const;

    bool readFromSocket();
    bool writeToSocket();

    void setKeepAlive(bool value);
    void markCloseAfterWrite();
    bool wantsWrite() const;
    bool shouldCloseAfterWrite() const;

    bool addToEpoll(int epfd) const;
    bool modEpoll(int epfd, bool want_write) const;

    std::string in_buffer;
    std::string out_buffer;

private:
    int fd;
    int listen_fd;
    bool keep_alive;
    bool close_after_write;
};

#endif
