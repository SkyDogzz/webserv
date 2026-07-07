#include "../../include/network/Connection.hpp"
#include "../../include/utils/Utils.hpp"
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>

static long currentTimeMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long>(tv.tv_sec) * 1000L + static_cast<long>(tv.tv_usec / 1000L);
}

Connection::Connection()
    : fd(-1)
    , listen_fd(-1)
    , keep_alive(false)
    , close_after_write(false)
    , last_activity_ms(currentTimeMs())
    , request_start_ms(currentTimeMs())
{
}

Connection::Connection(int fd, int listen_fd)
    : fd(fd)
    , listen_fd(listen_fd)
    , keep_alive(false)
    , close_after_write(false)
    , last_activity_ms(currentTimeMs())
    , request_start_ms(currentTimeMs())
{
    Utils::makeNonBlocking(fd);
}

void Connection::init(int fd, int listen_fd)
{
    Utils::closeFdSafe(this->fd);
    this->fd = fd;
    this->listen_fd = listen_fd;
    keep_alive = false;
    close_after_write = false;
    last_activity_ms = currentTimeMs();
    request_start_ms = currentTimeMs();
    Utils::makeNonBlocking(fd);
}

Connection::~Connection()
{
    Utils::closeFdSafe(fd);
}

int Connection::getFd() const { return fd; }

int Connection::getListenFd() const { return listen_fd; }

bool Connection::readFromSocket()
{
    char buffer[4096];
    ssize_t read_bytes = recv(fd, buffer, sizeof(buffer), 0);

    if (read_bytes > 0) {
        in_buffer.append(buffer, read_bytes);
        return true;
    }
    if (read_bytes == 0) {
        std::cerr << "recv EOF on fd=" << fd << std::endl;
        return false;
    }
    return true;
}

bool Connection::writeToSocket()
{
    if (out_buffer.empty())
        return true;

    ssize_t sent = send(fd, out_buffer.c_str(), out_buffer.size(), 0);
    if (sent > 0) {
        out_buffer.erase(0, sent);
        return true;
    }
    if (sent == 0) {
        std::cerr << "send returned 0 on fd=" << fd << std::endl;
        return false;
    }
    return true;
}

void Connection::setKeepAlive(bool value) { keep_alive = value; }

void Connection::markCloseAfterWrite() { close_after_write = true; }

bool Connection::wantsWrite() const { return !out_buffer.empty(); }

bool Connection::keepAlive() const { return keep_alive; }

bool Connection::closeAfterWriteRequested() const { return close_after_write; }

bool Connection::shouldCloseAfterWrite() const
{
    return close_after_write && !keep_alive && out_buffer.empty();
}

void Connection::markActivity() { last_activity_ms = currentTimeMs(); }

void Connection::markRequestStart() { request_start_ms = currentTimeMs(); }

bool Connection::isHeaderTimedOut(long now_ms, long timeout_ms) const
{
    if (timeout_ms <= 0)
        return false;
    return in_buffer.find("\r\n\r\n") == std::string::npos && (now_ms - request_start_ms) > timeout_ms;
}

bool Connection::isBodyTimedOut(long now_ms, long timeout_ms) const
{
    if (timeout_ms <= 0)
        return false;
    return in_buffer.find("\r\n\r\n") != std::string::npos && (now_ms - request_start_ms) > timeout_ms;
}

bool Connection::isIdleTimedOut(long now_ms, long timeout_ms) const
{
    if (timeout_ms <= 0)
        return false;
    return keep_alive && out_buffer.empty() && (now_ms - last_activity_ms) > timeout_ms;
}

long Connection::getLastActivityMs() const { return last_activity_ms; }

long Connection::getRequestStartMs() const { return request_start_ms; }

bool Connection::addToEpoll(int epfd) const
{
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Connection::modEpoll(int epfd, bool want_write) const
{
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | (want_write ? static_cast<uint32_t>(EPOLLOUT) : 0);
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == 0;
}
