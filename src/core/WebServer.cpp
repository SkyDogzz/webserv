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
    , _config(NULL)
{
    signal(SIGINT, &WebServer::sigintHandler);
}

WebServer::~WebServer() { }

WebServer& WebServer::getInstance()
{
    static WebServer _instance;
    return _instance;
}

void WebServer::appliConfig(Config& config)
{
    config.validate();
    _config = &config;
}

const Config* WebServer::getConfig() const { return _config; }

void WebServer::run() { _event_loop.run(); }

bool WebServer::isRunning() { return _running; }
