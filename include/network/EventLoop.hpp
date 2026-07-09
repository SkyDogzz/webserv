#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "../config/Config.hpp"

/*
    EventLoop
    ---------
    Wrapper around epoll().

    Purpose:
    - Wait for socket readiness.
    - Report readable/writable events.
    - No HTTP logic.
    - No request parsing.

    Purely I/O multiplexing.
*/

class Config;

class EventLoop {
public:
    static void run(const Config& config);
};

#endif
