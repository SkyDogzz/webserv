#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

/*
    HttpRequest
    -----------
    Parsed representation of an HTTP request.

    Contains:
    - Method
    - Path
    - Version
    - Headers
    - Body

    Pure data object.
*/

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

#endif
