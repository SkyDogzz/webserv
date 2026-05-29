#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "../config/Config.hpp"
#include "../network/EventLoop.hpp"

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
    EventLoop _event_loop;

    static void sigintHandler(int signal);

public:
    ~WebServer();

    static WebServer& getInstance();
    void appliConfig(Config& config);
    void run();
    bool isRunning();
};

#endif
