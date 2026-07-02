#include "../../include/config/Config.hpp"

#include <map>
#include <sstream>
#include <stdexcept>

namespace {

static void throwConfigError(const std::string& message)
{
    throw std::runtime_error("Config error: " + message);
}

}

Config::Config(const std::string& filename) { (void)filename; }

void Config::validate() const
{
    std::map<int, std::size_t> seen_ports;
    for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
        if (it->port < 1 || it->port > 65535) {
            std::ostringstream oss;
            oss << "invalid port " << it->port;
            throwConfigError(oss.str());
        }

        std::pair<std::map<int, std::size_t>::const_iterator, bool> inserted = seen_ports.insert(
            std::make_pair(it->port, seen_ports.size()));
        if (!inserted.second) {
            std::ostringstream oss;
            oss << "duplicate listen port " << it->port;
            throwConfigError(oss.str());
        }
    }
}
