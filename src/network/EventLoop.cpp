#include "../../include/network/EventLoop.hpp"
#include "../../include/core/WebServer.hpp"
#include "../../include/http/HttpRequestParser.hpp"
#include "../../include/network/Connection.hpp"
#include "../../include/network/ListeningSocket.hpp"
#include "handlers/StaticHandler.hpp"
#include "http/HttpResponse.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <vector>

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

    size_t pos = headers.find("Content-Length:");
    if (pos == std::string::npos)
        return buffer.size() >= body_start;

    pos += std::strlen("Content-Length:");
    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t'))
        ++pos;

    std::string len_str;
    while (pos < headers.size() && headers[pos] >= '0' && headers[pos] <= '9') {
        len_str += headers[pos];
        ++pos;
    }
    if (len_str.empty())
        return false;

    std::istringstream iss(len_str);
    iss >> content_length;
    return buffer.size() >= body_start + content_length;
}

static std::string buildSimpleResponse(int status_code, const std::string& body, bool keep_alive)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << status_code << " ";
    if (status_code == 200)
        out << "OK";
    else if (status_code == 400)
        out << "Bad Request";
    else if (status_code == 404)
        out << "Not Found";
    else
        out << "Error";
    out << "\r\n";
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

void EventLoop::run(Config* config)
{
    if (!config) {
        std::cerr << "No configuration provided to EventLoop" << std::endl;
        return;
    }

    std::vector<ListeningSocket> listens;
    for (size_t i = 0; i < config->servers.size(); ++i) {
        std::ostringstream oss;
        oss << config->servers[i].port;
        listens.push_back(ListeningSocket(config->servers[i].host, oss.str(), 128));
    }

    if (listens.empty()) {
        std::cerr << "No servers configured" << std::endl;
        return;
    }

    std::map<int, size_t> listen_map;
    for (size_t i = 0; i < listens.size(); ++i) {
        if (listens[i].getFd() == -1) {
            std::cerr << "Failed to open listening socket on port " << listens[i].getPort() << std::endl;
            continue;
        }
        std::cout << "Listening socket ready on port " << listens[i].getPort() << " fd=" << listens[i].getFd()
                  << std::endl;
        listen_map[listens[i].getFd()] = i;
    }

    if (listen_map.empty()) {
        std::cerr << "No listening sockets could be opened" << std::endl;
        return;
    }

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        std::cerr << "epoll_create1 failed: " << std::strerror(errno) << std::endl;
        return;
    }

    for (std::map<int, size_t>::iterator it = listen_map.begin(); it != listen_map.end(); ++it) {
        listens[it->second].addToEpoll(epfd);
    }

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
                    std::cout << "Accepted client fd=" << client_fd << " on listen fd=" << listen.getFd()
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
                    std::cout << "ok = " << ok << std::endl;
                    if (ok) {
                        std::map<std::string, std::string>::iterator hit = request.headers.find("Connection");
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
                        conn.out_buffer = buildSimpleResponse(400, "Bad Request", keep_alive);
                    else {
                        int listen_fd = conn.getListenFd();
                        size_t srv_idx = listen_map[listen_fd];
                        const ServerConfig& srv_cfg = config->servers[srv_idx];
                        
                        // Find the best matching location (longest prefix)
                        const LocationConfig* best_loc = NULL;
                        for (size_t j = 0; j < srv_cfg.locations.size(); ++j) {
                            if (request.path.find(srv_cfg.locations[j].path) == 0) {
                                if (!best_loc || srv_cfg.locations[j].path.length() > best_loc->path.length()) {
                                    best_loc = &srv_cfg.locations[j];
                                }
                            }
                        }

                        std::string root = srv_cfg.root;
                        if (best_loc && !best_loc->root.empty()) {
                            root = best_loc->root;
                        }
                        if (root.empty()) {
                            root = "./";
                        }

                        StaticHandler staticHandler(root);
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
}
