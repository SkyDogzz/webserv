#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <map>
#include <sstream>
#include <string>

/*
    HttpResponse
    ------------
    Represents an HTTP response to send to the client.

    Contains:
    - Status code
    - Headers
    - Body

    Can be serialized into raw HTTP format.
*/

class HttpResponse {
public:
    int status_code;
    std::map<std::string, std::string> headers;
    std::string body;

    static HttpResponse makeText(int status_code, const std::string& body, const std::string& content_type,
        bool keep_alive);
    static HttpResponse makeError(int status_code, bool keep_alive);

    std::string toString() const;
};

#endif
