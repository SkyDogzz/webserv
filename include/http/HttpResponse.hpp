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

    std::string toString() const;
};

#endif
