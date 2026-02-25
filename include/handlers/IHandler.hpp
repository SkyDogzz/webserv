#ifndef IHANDLER_HPP
#define IHANDLER_HPP

#include "../http/HttpRequest.hpp"
#include "../http/HttpResponse.hpp"

/*
    IHandler
    --------
    Interface for request handlers.

    Implementations:
    - Static file serving
    - CGI execution
    - Upload handling
    - Delete handling
    - Error responses

    Keeps business logic separate from networking.
*/

class IHandler {
public:
    virtual ~IHandler() {}
    virtual HttpResponse handle(const HttpRequest& request) = 0;
};

#endif
