#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HttpRequest.hpp"

/*
    HttpRequestParser
    -----------------
    Incremental parser for HTTP requests.

    Purpose:
    - Consume raw bytes from a Connection buffer.
    - Detect when a full request is received.
    - Handle partial reads safely.

    Essential for non-blocking servers.
*/

class HttpRequestParser {
public:
    static bool parse(const std::string& buffer, HttpRequest& request);

    class FirstLineInvalidException : public std::exception {
        virtual const char* what() const throw() { return "First line invalid exception"; }
    };
};

#endif
