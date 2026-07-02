#include "../../include/network/ListeningSocket.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

ListeningSocket::ListeningSocket()
    : fd(-1)
{
}

ListeningSocket::ListeningSocket(const std::string& host, const std::string& port, int backlog)
    : fd(-1)
    , host_(host)
    , port_(port)
{
    open(host, port, backlog);
}

ListeningSocket::ListeningSocket(const ListeningSocket& other)
    : fd(-1)
    , host_(other.host_)
    , port_(other.port_)
{
    if (other.fd != -1)
        fd = dup(other.fd);
}

ListeningSocket& ListeningSocket::operator=(const ListeningSocket& other)
{
    if (this == &other)
        return *this;
    if (fd != -1)
        close(fd);
    fd = -1;
    host_ = other.host_;
    port_ = other.port_;
    if (other.fd != -1)
        fd = dup(other.fd);
    return *this;
}

ListeningSocket::~ListeningSocket()
{
    if (fd != -1)
        close(fd);
}

bool ListeningSocket::open(const std::string& host, const std::string& port, int backlog)
{
    host_ = host;
    port_ = port;

    struct addrinfo hints;
    struct addrinfo* result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const char* host_cstr = host.empty() ? NULL : host.c_str();
    int s = getaddrinfo(host_cstr, port.c_str(), &hints, &result);
    if (s != 0) {
        std::cerr << "getaddrinfo failed for host=" << (host.empty() ? "*" : host) << " port=" << port << ": "
                  << gai_strerror(s) << std::endl;
        return false;
    }

    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        int socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue;

        int opt = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << std::endl;
        }

        char hostbuf[NI_MAXHOST];
        char servbuf[NI_MAXSERV];
        std::memset(hostbuf, 0, sizeof(hostbuf));
        std::memset(servbuf, 0, sizeof(servbuf));
        int gni = getnameinfo(rp->ai_addr, rp->ai_addrlen, hostbuf, sizeof(hostbuf), servbuf, sizeof(servbuf),
            NI_NUMERICHOST | NI_NUMERICSERV);

        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(socket_fd, backlog) == -1) {
                std::cerr << "listen failed on " << (gni == 0 ? hostbuf : "*") << ":" << (gni == 0 ? servbuf : port)
                          << ": " << std::strerror(errno) << std::endl;
                close(socket_fd);
                continue;
            }
            Utils::makeNonBlocking(socket_fd);
            fd = socket_fd;
            DEBUG_LOG << "Listening on " << (gni == 0 ? hostbuf : "*") << ":" << (gni == 0 ? servbuf : port)
                      << " fd=" << fd << std::endl;
            break;
        }

        std::cerr << "bind failed on " << (gni == 0 ? hostbuf : "*") << ":" << (gni == 0 ? servbuf : port) << ": "
                  << std::strerror(errno) << std::endl;
        close(socket_fd);
    }

    freeaddrinfo(result);
    return fd != -1;
}

int ListeningSocket::acceptClient() const
{
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return -1;
    if (client_fd == -1)
        return -1;
    Utils::makeNonBlocking(client_fd);
    return client_fd;
}

bool ListeningSocket::addToEpoll(int epfd) const
{
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

int ListeningSocket::getFd() const { return fd; }

std::string ListeningSocket::getHost() const { return host_; }

std::string ListeningSocket::getPort() const { return port_; }
