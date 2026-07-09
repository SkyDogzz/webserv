#include "../../include/config/Config.hpp"
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <stdexcept>

// Helper to convert size string (e.g., 10M, 1M, 250K) to bytes
static size_t parseBodySize(const std::string& sizeStr) {
    if (sizeStr.empty()) return 1048576; // 1MB default
    
    std::stringstream ss(sizeStr);
    double value;
    ss >> value;
    
    char unit = '\0';
    if (ss >> unit) {
        if (unit == 'K' || unit == 'k') {
            value *= 1024;
        } else if (unit == 'M' || unit == 'm') {
            value *= 1024 * 1024;
        } else if (unit == 'G' || unit == 'g') {
            value *= 1024 * 1024 * 1024;
        }
    }
    return static_cast<size_t>(value);
}

Config::Config(JsonValue *obj) {
    if (!obj || obj->getType() != JSON_OBJET) {
        throw std::runtime_error("Config root must be an object");
    }

    const std::map<std::string, JsonValue*>& root = obj->asObject();
    std::map<std::string, JsonValue*>::const_iterator serversIt = root.find("servers");

    if (serversIt == root.end() || serversIt->second->getType() != JSON_ARRAY) {
        throw std::runtime_error("Config must contain a 'servers' array");
    }

    const std::vector<JsonValue*>& serversArr = serversIt->second->asArray();
    for (size_t i = 0; i < serversArr.size(); ++i) {
        JsonValue* serverJson = serversArr[i];
        if (!serverJson || serverJson->getType() != JSON_OBJET) {
            continue;
        }

        const std::map<std::string, JsonValue*>& serverMap = serverJson->asObject();
        ServerConfig s;

        // Parse host/port via "listen" directive
        std::map<std::string, JsonValue*>::const_iterator listenIt = serverMap.find("listen");
        if (listenIt != serverMap.end()) {
            if (listenIt->second->getType() == JSON_STRING) {
                std::string listenStr = listenIt->second->asString();
                size_t colonPos = listenStr.find(':');
                if (colonPos != std::string::npos) {
                    s.host = listenStr.substr(0, colonPos);
                    std::string portStr = listenStr.substr(colonPos + 1);
                    if (portStr.empty()) throw std::runtime_error("Empty port in listen directive");
                    s.port = std::atoi(portStr.c_str());
                } else {
                    s.port = std::atoi(listenStr.c_str());
                }
            } else if (listenIt->second->getType() == JSON_NUMBER) {
                s.port = static_cast<int>(listenIt->second->asNumber());
            }
            if (s.port <= 0 || s.port > 65535) {
                throw std::runtime_error("Invalid port in configuration");
            }
        }

        // Parse server_name
        std::map<std::string, JsonValue*>::const_iterator nameIt = serverMap.find("server_name");
        if (nameIt != serverMap.end() && nameIt->second->getType() == JSON_STRING) {
            s.server_name = nameIt->second->asString();
        }

        // Parse root (server-wide)
        std::map<std::string, JsonValue*>::const_iterator rootIt = serverMap.find("root");
        if (rootIt != serverMap.end() && rootIt->second->getType() == JSON_STRING) {
            s.root = rootIt->second->asString();
        }

        // Parse index (server-wide)
        std::map<std::string, JsonValue*>::const_iterator idxIt = serverMap.find("index");
        if (idxIt != serverMap.end() && idxIt->second->getType() == JSON_STRING) {
            s.index = idxIt->second->asString();
        }

        // Parse client_max_body_size
        std::map<std::string, JsonValue*>::const_iterator bodySizeIt = serverMap.find("client_max_body_size");
        if (bodySizeIt != serverMap.end()) {
            if (bodySizeIt->second->getType() == JSON_NUMBER) {
                s.client_max_body_size = static_cast<size_t>(bodySizeIt->second->asNumber());
            } else if (bodySizeIt->second->getType() == JSON_STRING) {
                s.client_max_body_size = parseBodySize(bodySizeIt->second->asString());
            }
        }

        // Parse error_pages
        std::map<std::string, JsonValue*>::const_iterator errIt = serverMap.find("error_pages");
        if (errIt != serverMap.end() && errIt->second->getType() == JSON_OBJET) {
            const std::map<std::string, JsonValue*>& errMap = errIt->second->asObject();
            for (std::map<std::string, JsonValue*>::const_iterator it = errMap.begin(); it != errMap.end(); ++it) {
                int errCode = std::atoi(it->first.c_str());
                if (it->second->getType() == JSON_STRING) {
                    s.error_pages[errCode] = it->second->asString();
                }
            }
        }

        // Parse locations
        std::map<std::string, JsonValue*>::const_iterator locsIt = serverMap.find("locations");
        if (locsIt != serverMap.end() && locsIt->second->getType() == JSON_OBJET) {
            const std::map<std::string, JsonValue*>& locsMap = locsIt->second->asObject();
            for (std::map<std::string, JsonValue*>::const_iterator it = locsMap.begin(); it != locsMap.end(); ++it) {
                LocationConfig loc;
                loc.path = it->first;

                if (it->second->getType() == JSON_OBJET) {
                    const std::map<std::string, JsonValue*>& locData = it->second->asObject();

                    // methods
                    std::map<std::string, JsonValue*>::const_iterator methIt = locData.find("methods");
                    if (methIt != locData.end() && methIt->second->getType() == JSON_ARRAY) {
                        const std::vector<JsonValue*>& methArr = methIt->second->asArray();
                        for (size_t j = 0; j < methArr.size(); ++j) {
                            if (methArr[j]->getType() == JSON_STRING) {
                                loc.allowed_methods.insert(methArr[j]->asString());
                            }
                        }
                    }

                    // root
                    std::map<std::string, JsonValue*>::const_iterator locRootIt = locData.find("root");
                    if (locRootIt != locData.end() && locRootIt->second->getType() == JSON_STRING) {
                        loc.root = locRootIt->second->asString();
                    } else {
                        loc.root = s.root;
                    }

                    // index
                    std::map<std::string, JsonValue*>::const_iterator locIdxIt = locData.find("index");
                    if (locIdxIt != locData.end() && locIdxIt->second->getType() == JSON_STRING) {
                        loc.index = locIdxIt->second->asString();
                    } else {
                        loc.index = s.index;
                    }

                    // autoindex
                    std::map<std::string, JsonValue*>::const_iterator autoIt = locData.find("autoindex");
                    if (autoIt != locData.end() && autoIt->second->getType() == JSON_BOOL) {
                        loc.autoindex = autoIt->second->asBool();
                    }

                    // return (redirection)
                    std::map<std::string, JsonValue*>::const_iterator retIt = locData.find("return");
                    if (retIt != locData.end() && retIt->second->getType() == JSON_OBJET) {
                        const std::map<std::string, JsonValue*>& retData = retIt->second->asObject();
                        std::map<std::string, JsonValue*>::const_iterator codeIt = retData.find("code");
                        std::map<std::string, JsonValue*>::const_iterator urlIt = retData.find("url");
                        if (codeIt != retData.end() && codeIt->second->getType() == JSON_NUMBER) {
                            loc.redirect_code = static_cast<int>(codeIt->second->asNumber());
                        }
                        if (urlIt != retData.end() && urlIt->second->getType() == JSON_STRING) {
                            loc.redirect_url = urlIt->second->asString();
                        }
                    }

                    // CGI
                    std::map<std::string, JsonValue*>::const_iterator cgiPathIt = locData.find("cgi_path");
                    std::map<std::string, JsonValue*>::const_iterator cgiExtIt = locData.find("cgi_ext");
                    if (cgiPathIt != locData.end() && cgiExtIt != locData.end() &&
                        cgiPathIt->second->getType() == JSON_STRING && cgiExtIt->second->getType() == JSON_STRING) {
                        loc.cgi[cgiExtIt->second->asString()] = cgiPathIt->second->asString();
                    }

                    // Upload
                    std::map<std::string, JsonValue*>::const_iterator uploadEnableIt = locData.find("upload_enable");
                    std::map<std::string, JsonValue*>::const_iterator uploadStoreIt = locData.find("upload_store");
                    bool uploadEnabled = false;
                    if (uploadEnableIt != locData.end()) {
                        if (uploadEnableIt->second->getType() == JSON_BOOL) {
                            uploadEnabled = uploadEnableIt->second->asBool();
                        }
                    }
                    if (uploadEnabled && uploadStoreIt != locData.end() && uploadStoreIt->second->getType() == JSON_STRING) {
                        loc.upload_dir = uploadStoreIt->second->asString();
                    }
                }
                s.locations.push_back(loc);
            }
        }
        this->servers.push_back(s);
    }
}
