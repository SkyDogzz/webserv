#include "../../include/network/EventLoop.hpp"
#include "../../include/core/WebServer.hpp"
#include "../../include/core/Router.hpp"
#include "../../include/http/HttpRequestParser.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/network/Connection.hpp"
#include "../../include/network/ListeningSocket.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include "handlers/StaticHandler.hpp"
#include "http/HttpResponse.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <set>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

static bool parseChunkSizeLine(const std::string& line, std::size_t& chunk_size)
{
    std::size_t pos = 0;
    chunk_size = 0;

    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos == line.size())
        return false;

    for (; pos < line.size(); ++pos) {
        char c = line[pos];
        if (c == ';')
            return true;
        if (c >= '0' && c <= '9') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(10 + c - 'A');
        } else if (c == ' ' || c == '\t') {
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
                ++pos;
            return pos == line.size() || line[pos] == ';';
        } else {
            return false;
        }
    }
    return true;
}

static bool consumeChunkedBody(const std::string& body, std::size_t& raw_body_length, bool& invalid)
{
    std::size_t pos = 0;
    invalid = false;

    while (true) {
        std::size_t line_end = body.find('\n', pos);
        if (line_end == std::string::npos)
            return false;

        std::string size_line = body.substr(pos, line_end - pos);
        if (!size_line.empty() && size_line[size_line.size() - 1] == '\r')
            size_line.erase(size_line.size() - 1);

        std::size_t chunk_size = 0;
        if (!parseChunkSizeLine(size_line, chunk_size)) {
            invalid = true;
            raw_body_length = body.size();
            return true;
        }

        pos = line_end + 1;
        if (body.size() < pos + chunk_size)
            return false;

        pos += chunk_size;
        if (pos >= body.size())
            return false;

        if (body[pos] == '\r') {
            if (pos + 1 >= body.size())
                return false;
            if (body[pos + 1] != '\n') {
                invalid = true;
                raw_body_length = body.size();
                return true;
            }
            pos += 2;
        } else if (body[pos] == '\n') {
            ++pos;
        } else {
            invalid = true;
            raw_body_length = body.size();
            return true;
        }

        if (chunk_size == 0) {
            while (true) {
                std::size_t trailer_end = body.find('\n', pos);
                if (trailer_end == std::string::npos)
                    return false;
                std::string trailer_line = body.substr(pos, trailer_end - pos);
                if (!trailer_line.empty() && trailer_line[trailer_line.size() - 1] == '\r')
                    trailer_line.erase(trailer_line.size() - 1);
                pos = trailer_end + 1;
                if (trailer_line.empty()) {
                    raw_body_length = pos;
                    return true;
                }
            }
        }
    }
}

enum RequestFrameStatus {
    FRAME_INCOMPLETE,
    FRAME_COMPLETE,
    FRAME_INVALID
};

static bool parseStrictSize(const std::string& value, std::size_t& out)
{
    std::string trimmed = Utils::trimCopy(value);
    if (trimmed.empty())
        return false;

    out = 0;
    for (std::size_t i = 0; i < trimmed.size(); ++i) {
        char c = trimmed[i];
        if (c < '0' || c > '9')
            return false;
        std::size_t digit = static_cast<std::size_t>(c - '0');
        if (out > (static_cast<std::size_t>(-1) - digit) / 10)
            return false;
        out = out * 10 + digit;
    }
    return true;
}

static RequestFrameStatus requestComplete(const std::string& buffer, size_t& body_start, size_t& content_length)
{
    const std::size_t max_header_size = 16384;
    body_start = std::string::npos;
    content_length = 0;

    size_t header_end = buffer.find("\r\n\r\n");
    size_t sep_len = 4;
    if (header_end == std::string::npos) {
        header_end = buffer.find("\n\n");
        sep_len = 2;
    }
    if (header_end == std::string::npos) {
        if (buffer.size() > max_header_size)
            return FRAME_INVALID;
        return FRAME_INCOMPLETE;
    }

    if (header_end > max_header_size)
        return FRAME_INVALID;

    body_start = header_end + sep_len;
    std::string headers = buffer.substr(0, header_end);
    if (headers.size() > max_header_size)
        return FRAME_INVALID;

    bool saw_transfer_encoding = false;
    bool saw_content_length = false;
    std::string content_length_header;
    std::size_t start = 0;
    while (start <= headers.size()) {
        std::size_t end = headers.find('\n', start);
        std::size_t line_len = (end == std::string::npos) ? headers.size() - start : end - start;
        std::string line = headers.substr(start, line_len);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string normalized_key = Utils::toLowerCopy(key);
            if (normalized_key == "transfer-encoding") {
                std::size_t value_start = colon + 1;
                if (value_start < line.size() && line[value_start] == ' ')
                    ++value_start;
                std::string current_value = Utils::toLowerCopy(line.substr(value_start));
                if (current_value == "chunked")
                    saw_transfer_encoding = true;
            } else if (normalized_key == "content-length") {
                std::size_t value_start = colon + 1;
                if (value_start < line.size() && line[value_start] == ' ')
                    ++value_start;
                std::string current_value = line.substr(value_start);
                if (saw_content_length)
                    return FRAME_INVALID;
                saw_content_length = true;
                content_length_header = current_value;
            }
        }

        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    if (saw_transfer_encoding) {
        std::string body = buffer.substr(body_start);
        bool invalid = false;
        if (!consumeChunkedBody(body, content_length, invalid))
            return FRAME_INCOMPLETE;
        if (invalid)
            return FRAME_INVALID;
        return FRAME_COMPLETE;
    }

    if (!saw_content_length)
        return FRAME_COMPLETE;

    if (!parseStrictSize(content_length_header, content_length))
        return FRAME_INVALID;

    if (buffer.size() < body_start + content_length)
        return FRAME_INCOMPLETE;
    return FRAME_COMPLETE;
}

static std::string joinAllowHeader(const RequestContext& context)
{
    std::ostringstream out;
    bool first = true;
    if (context.allowed_methods.empty()) {
        out << "GET, HEAD";
        return out.str();
    }
    for (std::set<std::string>::const_iterator it = context.allowed_methods.begin(); it != context.allowed_methods.end();
         ++it) {
        if (!first)
            out << ", ";
        out << *it;
        first = false;
    }
    return out.str();
}

static bool readWholeFile(const std::string& path, std::string& content)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
        return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    return true;
}

static HttpResponse buildErrorResponse(int status_code, bool keep_alive, const RequestContext* context)
{
    HttpResponse response = HttpResponse::makeError(status_code, keep_alive);
    response.headers["Connection"] = keep_alive ? "keep-alive" : "close";

    if (status_code == 405 && context != NULL) {
        response.headers["Allow"] = joinAllowHeader(*context);
    }

    if (context != NULL) {
        std::map<int, std::string>::const_iterator it = context->error_pages.find(status_code);
        if (it != context->error_pages.end()) {
            std::string custom_body;
            if (readWholeFile(it->second, custom_body)) {
                response.body = custom_body;
                response.headers["Content-Type"] = "text/html";
                std::ostringstream len;
                len << response.body.size();
                response.headers["Content-Length"] = len.str();
            }
        }
    }

    return response;
}

static bool isMethodAllowed(const RequestContext& context, const std::string& method)
{
    if (context.allowed_methods.empty())
        return method == "GET" || method == "HEAD";
    return context.allowed_methods.find(method) != context.allowed_methods.end();
}

static long currentTimeMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<long>(tv.tv_sec) * 1000L + static_cast<long>(tv.tv_usec / 1000L);
}

static void closeConnection(int epfd, std::map<int, Connection>& conns, int fd);

static void closeListeningSocket(
    int epfd, std::vector<ListeningSocket>& listens, std::map<int, size_t>& listen_map, int fd)
{
    std::map<int, size_t>::iterator it = listen_map.find(fd);
    if (it == listen_map.end())
        return;

    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    listens[it->second].closeSocket();
    listen_map.erase(it);
}

static void expireTimedOutConnections(int epfd, std::map<int, Connection>& conns)
{
    const long header_timeout_ms = 15000;
    const long body_timeout_ms = 15000;
    const long idle_timeout_ms = 30000;
    const long now_ms = currentTimeMs();

    std::vector<int> expired;
    for (std::map<int, Connection>::iterator it = conns.begin(); it != conns.end(); ++it) {
        if (it->second.isHeaderTimedOut(now_ms, header_timeout_ms)
            || it->second.isBodyTimedOut(now_ms, body_timeout_ms)
            || it->second.isIdleTimedOut(now_ms, idle_timeout_ms)) {
            expired.push_back(it->first);
        }
    }

    for (std::size_t i = 0; i < expired.size(); ++i)
        closeConnection(epfd, conns, expired[i]);
}

static void closeConnection(int epfd, std::map<int, Connection>& conns, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    std::map<int, Connection>::iterator it = conns.find(fd);
    if (it != conns.end())
        conns.erase(it);
}

void EventLoop::run()
{
    std::vector<ListeningSocket> listens;
    listens.push_back(ListeningSocket("", "8080", 128));
    listens.push_back(ListeningSocket("", "8081", 128));

    DEBUG_LOG << "listening to socket 8080 8081" << std::endl;

    std::map<int, size_t> listen_map;
    for (size_t i = 0; i < listens.size(); ++i) {
        if (listens[i].getFd() == -1) {
            std::cerr << "Failed to open listening socket on port " << listens[i].getPort() << std::endl;
            return;
        }
        DEBUG_LOG << "Listening socket ready on port " << listens[i].getPort() << " fd=" << listens[i].getFd()
                  << std::endl;
        listen_map[listens[i].getFd()] = i;
    }

    int epfd = epoll_create(1);
    if (epfd == -1) {
        std::cerr << "epoll_create failed: " << std::strerror(errno) << std::endl;
        return;
    }

    for (size_t i = 0; i < listens.size(); ++i)
        listens[i].addToEpoll(epfd);

    std::map<int, Connection> conns;
    const Config* config = WebServer::getInstance().getConfig();

    while (WebServer::getInstance().isRunning()) {
        expireTimedOutConnections(epfd, conns);
        if (listen_map.empty() && conns.empty())
            break;
        struct epoll_event events[64];
        int n = epoll_wait(epfd, events, 64, -1);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            std::cerr << "epoll_wait failed: " << std::strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (listen_map.find(fd) != listen_map.end()) {
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    closeListeningSocket(epfd, listens, listen_map, fd);
                    continue;
                }
                ListeningSocket& listen = listens[listen_map[fd]];
                for (;;) {
                    int client_fd = listen.acceptClient();
                    if (client_fd == -1)
                        break;
                    DEBUG_LOG << "Accepted client fd=" << client_fd << " on listen fd=" << listen.getFd()
                              << " port=" << listen.getPort() << std::endl;
                    Connection& conn = conns[client_fd];
                    conn.init(client_fd, listen.getFd());
                    conn.addToEpoll(epfd);
                }
                continue;
            }

            std::map<int, Connection>::iterator it = conns.find(fd);
            if (it == conns.end())
                continue;
            Connection& conn = it->second;

            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                closeConnection(epfd, conns, fd);
                continue;
            }

            if (events[i].events & EPOLLIN) {
                bool was_empty = conn.in_buffer.empty();
                bool peer_closed = false;
                if (!conn.readFromSocket())
                    peer_closed = true;
                if (peer_closed && conn.in_buffer.empty()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }
                if (was_empty)
                    conn.markRequestStart();
                conn.markActivity();
                if (peer_closed)
                    conn.markCloseAfterWrite();
                while (true) {
                    size_t body_start = 0;
                    size_t content_length = 0;
                    RequestFrameStatus frame = requestComplete(conn.in_buffer, body_start, content_length);
                    if (frame == FRAME_INCOMPLETE)
                        break;

                    if (frame == FRAME_INVALID) {
                        HttpResponse response = buildErrorResponse(400, false, NULL);
                        conn.out_buffer += response.toString();
                        conn.in_buffer.clear();
                        conn.markCloseAfterWrite();
                        break;
                    }

                    std::string request_raw = conn.in_buffer.substr(0, body_start + content_length);
                    conn.in_buffer.erase(0, body_start + content_length);

                    HttpRequest request;
                    bool ok = false;
                    try {
                        ok = HttpRequestParser::parse(request_raw, request);
                    } catch (std::exception& e) {
                        std::cerr << "Parse error: " << e.what() << std::endl;
                    }

                    DEBUG_LOG << "ok = " << ok << std::endl;
                    if (!ok) {
                        HttpResponse response = buildErrorResponse(400, false, NULL);
                        conn.out_buffer += response.toString();
                        conn.markCloseAfterWrite();
                        conn.in_buffer.clear();
                        break;
                    }

                    bool keep_alive = request.keep_alive;
                    conn.setKeepAlive(keep_alive);
                    if (!keep_alive)
                        conn.markCloseAfterWrite();

                    HttpResponse response;
                    response.status_code = 500;

                    if (config != NULL && !config->servers.empty()) {
                        std::map<int, size_t>::const_iterator listen_it = listen_map.find(conn.getListenFd());
                        int listen_port = 0;
                        if (listen_it != listen_map.end())
                            listen_port = std::atoi(listens[listen_it->second].getPort().c_str());

                        Router router(*config);
                        RequestContext context;
                        if (router.resolve(listen_port, request, context)) {
                            size_t max_body = context.server != NULL ? context.server->client_max_body_size : 0;
                            if (max_body > 0 && request.body.size() > max_body) {
                                response = buildErrorResponse(413, keep_alive, &context);
                            } else if (!isMethodAllowed(context, request.method)) {
                                response = buildErrorResponse(405, keep_alive, &context);
                            } else {
                                StaticHandler staticHandler(context);
                                response = staticHandler.handle(request);
                                response.headers["Connection"] = keep_alive ? "keep-alive" : "close";
                            }
                        } else {
                            RequestContext fallback;
                            StaticHandler staticHandler(fallback);
                            response = staticHandler.handle(request);
                            response.headers["Connection"] = keep_alive ? "keep-alive" : "close";
                        }
                    } else {
                        RequestContext fallback;
                        StaticHandler staticHandler(fallback);
                        response = staticHandler.handle(request);
                        response.headers["Connection"] = keep_alive ? "keep-alive" : "close";
                    }

                    conn.out_buffer += response.toString();
                    if (!keep_alive)
                        break;
                    if (conn.in_buffer.empty())
                        break;
                    conn.markRequestStart();
                }

                if (peer_closed && !conn.wantsWrite()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }
                conn.modEpoll(epfd, conn.wantsWrite());
            }

            if (events[i].events & EPOLLOUT) {
                if (!conn.writeToSocket()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }
                conn.markActivity();
                conn.modEpoll(epfd, conn.wantsWrite());
                if (conn.shouldCloseAfterWrite()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }
            }
        }
    }
    close(epfd);
}
