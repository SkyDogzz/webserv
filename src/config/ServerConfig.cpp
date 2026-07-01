#include "../../include/config/ServerConfig.hpp"

ServerConfig::ServerConfig()
    : host("")
    , port(80)
    , server_name("")
    , root(".")
    , index("index.html")
    , client_max_body_size(1024 * 1024)
{
}
