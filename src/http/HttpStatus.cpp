#include "../../include/http/HttpStatus.hpp"

const char* httpReasonPhrase(int status_code)
{
    switch (status_code) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    case 504:
        return "Gateway Timeout";
    default:
        return "Error";
    }
}
