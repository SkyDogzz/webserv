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

private:
    WebServer();
    WebServer(const WebServer& other);
    WebServer& operator=(const WebServer& other);

    bool _running;

    static void sigintHandler(int signal);

public:
    ~WebServer();

    static WebServer& getInstance();
    void appliConfig(Config& config);
    void run();
};

#endif
