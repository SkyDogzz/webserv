#include "../../include/core/WebServer.hpp"

#include <csignal>
#include <iostream>

void WebServer::sigintHandler(int signal)
{
    std::cerr << "SIGINT triggered (" << signal << "): cleaning everything before quitting" << std::endl;
    WebServer::getInstance()._running = false;
}

WebServer::WebServer()
    : _running(true)
{
    signal(SIGINT, &WebServer::sigintHandler);
}

WebServer::~WebServer() { }

WebServer& WebServer::getInstance()
{
    static WebServer _instance;
    return _instance;
}

void WebServer::appliConfig(Config& config) { (void)config; }

void WebServer::run()
{
    while (_running) {
        //std::cout << "doing things" << std::endl;
    }
}
