#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "../config/Config.hpp"

/*
    WebServer
    ---------
    Main application engine.

    Responsibilities:
    - Load configuration.
    - Initialize listening sockets.
    - Run the EventLoop.
    - Coordinate connections and request handling.

    This is the orchestrator of the whole system.
*/

class WebServer {
public:
    WebServer(const Config& config);
    void run();
};

#endif
