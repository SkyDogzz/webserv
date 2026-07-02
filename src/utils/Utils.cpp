#include "../../include/utils/Utils.hpp"

#include <cctype>
#include <fcntl.h>
#include <unistd.h>

namespace Utils {

std::string toLowerCopy(const std::string& value)
{
    std::string lower = value;
    for (std::size_t i = 0; i < lower.size(); ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
    return lower;
}

std::string trimCopy(const std::string& value)
{
    std::string::size_type start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;

    std::string::size_type end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;

    return value.substr(start, end - start);
}

bool makeNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return false;
    return true;
}

void closeFdSafe(int fd)
{
    if (fd != -1)
        close(fd);
}

}
