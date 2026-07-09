#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <iostream>
#include <string>
#include <sys/time.h>

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
    Connection(int client_fd, int listen_fd_arg);
    ~Connection();

    void init(int client_fd, int listen_fd_arg);
    int getFd() const;
    int getListenFd() const;

    bool readFromSocket();
    bool writeToSocket();

    void setKeepAlive(bool value);
    void markCloseAfterWrite();
    bool wantsWrite() const;
    bool keepAlive() const;
    bool closeAfterWriteRequested() const;
    bool shouldCloseAfterWrite() const;
    void markActivity();
    void markRequestStart();
    bool isHeaderTimedOut(long now_ms, long timeout_ms) const;
    bool isBodyTimedOut(long now_ms, long timeout_ms) const;
    bool isIdleTimedOut(long now_ms, long timeout_ms) const;
    long getLastActivityMs() const;
    long getRequestStartMs() const;

    bool addToEpoll(int epfd) const;
    bool modEpoll(int epfd, bool want_write) const;

    std::string in_buffer;
    std::string out_buffer;

private:
    int fd;
    int listen_fd;
    bool keep_alive;
    bool close_after_write;
    long last_activity_ms;
    long request_start_ms;
};

#endif
