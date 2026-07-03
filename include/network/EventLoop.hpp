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

class EventLoop {
public:
    void run(const Config& config);
};

#endif
