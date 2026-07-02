#include "../../include/config/LocationConfig.hpp"

LocationConfig::LocationConfig()
    : path("/")
    , root("")
    , index("")
    , autoindex(false)
    , redirect_code(0)
    , redirect_url("")
    , upload_dir("")
{
}

LocationConfig::~LocationConfig() { }
