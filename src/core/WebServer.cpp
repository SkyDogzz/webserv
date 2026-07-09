#include "../../include/core/WebServer.hpp"

#include <csignal>
#include <iostream>

void WebServer::sigintHandler(int signal)
{
    std::cerr << "SIGINT triggered (" << signal << "): cleaning everything before quitting" << std::endl;
    WebServer::getInstance()._running = false;
}

WebServer::WebServer()
    : _running(true), _config(NULL)
{
    signal(SIGINT, &WebServer::sigintHandler);
    signal(SIGPIPE, SIG_IGN);
}

WebServer::~WebServer() {
    if (_config) {
        delete _config;
    }
}

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

void WebServer::run()
{
    if (_config == NULL) {
        std::cerr << "No configuration loaded" << std::endl;
        return;
    }
    _event_loop.run(*_config);
}

bool WebServer::isRunning() const { return _running; }
