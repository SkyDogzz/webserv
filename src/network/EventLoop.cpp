#include "../../include/network/EventLoop.hpp"
#include "../../include/core/WebServer.hpp"
#include "../../include/http/HttpRequestParser.hpp"
#include <iostream>

static const char* buffer
    = "POST /users HTTP/1.1\nHost: example.com\nContent-Type: application/x-www-form-urlencoded\nContent-Length: "
      "49\n\nname=FirstName+LastName&email=bsmth%40example.com";

void EventLoop::run()
{
    static size_t loop_count = 1;
    while (WebServer::getInstance().isRunning()) {
        if (loop_count == 1) {
            std::string strBuffer(buffer);
            HttpRequest request;
            try {
                bool res = HttpRequestParser::parse(buffer, request);
                std::cout << res << std::endl;
            } catch (std::exception& e) {
                std::cerr << "Caught exception: " << e.what() << std::endl;
            }
        }
        loop_count++;
    }
}
