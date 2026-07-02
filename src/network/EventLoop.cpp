#include "../../include/network/EventLoop.hpp"
#include "../../include/core/WebServer.hpp"
#include "../../include/http/HttpRequestParser.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/network/Connection.hpp"
#include "../../include/network/ListeningSocket.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "handlers/StaticHandler.hpp"
#include "http/HttpResponse.hpp"
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

static std::string toLowerCopy(const std::string& value)
{
    std::string lower = value;
    for (std::size_t i = 0; i < lower.size(); ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
    return lower;
}

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

static bool requestComplete(const std::string& buffer, size_t& body_start, size_t& content_length)
{
    body_start = std::string::npos;
    content_length = 0;

    size_t header_end = buffer.find("\r\n\r\n");
    size_t sep_len = 4;
    if (header_end == std::string::npos) {
        header_end = buffer.find("\n\n");
        sep_len = 2;
    }
    if (header_end == std::string::npos)
        return false;

    body_start = header_end + sep_len;
    std::string headers = buffer.substr(0, header_end);

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
            std::string normalized_key = toLowerCopy(key);
            if (normalized_key == "transfer-encoding") {
                std::size_t value_start = colon + 1;
                if (value_start < line.size() && line[value_start] == ' ')
                    ++value_start;
                std::string current_value = toLowerCopy(line.substr(value_start));
                if (current_value == "chunked")
                    saw_transfer_encoding = true;
            } else if (normalized_key == "content-length") {
                std::size_t value_start = colon + 1;
                if (value_start < line.size() && line[value_start] == ' ')
                    ++value_start;
                std::string current_value = line.substr(value_start);
                if (saw_content_length)
                {
                    content_length = buffer.size() - body_start;
                    return true;
                }
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
            return false;
        if (invalid)
            return true;
        return true;
    }

    if (!saw_content_length)
        return buffer.size() >= body_start;

    std::string len_str;
    for (std::size_t i = 0; i < content_length_header.size(); ++i) {
        if (content_length_header[i] < '0' || content_length_header[i] > '9')
        {
            content_length = buffer.size() - body_start;
            return true;
        }
        len_str += content_length_header[i];
    }
    if (len_str.empty())
    {
        content_length = buffer.size() - body_start;
        return true;
    }

    std::istringstream iss(len_str);
    iss >> content_length;
    return buffer.size() >= body_start + content_length;
}

static std::string buildSimpleResponse(int status_code, const std::string& body, bool keep_alive)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << status_code << " " << httpReasonPhrase(status_code) << "\r\n";
    out << "Content-Type: text/plain\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    out << "\r\n";
    out << body;
    return out.str();
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

    while (WebServer::getInstance().isRunning()) {
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
                if (!conn.readFromSocket()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }

                size_t body_start = 0;
                size_t content_length = 0;
                if (requestComplete(conn.in_buffer, body_start, content_length)) {
                    std::string request_raw = conn.in_buffer.substr(0, body_start + content_length);
                    conn.in_buffer.erase(0, body_start + content_length);

                    HttpRequest request;
                    bool ok = false;
                    try {
                        ok = HttpRequestParser::parse(request_raw, request);
                    } catch (std::exception& e) {
                        std::cerr << "Parse error: " << e.what() << std::endl;
                    }

                    bool keep_alive = false;
                    DEBUG_LOG << "ok = " << ok << std::endl;
                    if (ok) {
                        std::map<std::string, std::string>::iterator hit = request.headers.find("connection");
                        if (hit != request.headers.end()) {
                            keep_alive = (hit->second == "keep-alive");
                        } else {
                            keep_alive = (request.version == "HTTP/1.1");
                        }
                    }
                    conn.setKeepAlive(keep_alive);
                    if (!keep_alive)
                        conn.markCloseAfterWrite();

                    if (!ok)
                        conn.out_buffer = buildSimpleResponse(400, httpReasonPhrase(400), keep_alive);
                    else {
                        StaticHandler staticHandler("./");
                        HttpResponse response = staticHandler.handle(request);
                        conn.out_buffer = response.toString();
                    }
                    conn.modEpoll(epfd, true);
                }
            }

            if (events[i].events & EPOLLOUT) {
                if (!conn.writeToSocket()) {
                    closeConnection(epfd, conns, fd);
                    continue;
                }
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
