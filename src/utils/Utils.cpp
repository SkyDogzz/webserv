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

std::string stripQueryCopy(const std::string& value)
{
    std::string result = value;
    std::string::size_type query = result.find('?');
    if (query != std::string::npos)
        result.erase(query);
    return result;
}

std::string normalizePathCopy(const std::string& value)
{
    std::string normalized = stripQueryCopy(value);
    if (normalized.empty() || normalized[0] != '/')
        normalized.insert(normalized.begin(), '/');
    while (normalized.size() > 1 && normalized[normalized.size() - 1] == '/')
        normalized.erase(normalized.size() - 1);
    return normalized;
}

std::string joinPathCopy(const std::string& base, const std::string& leaf)
{
    if (base.empty())
        return leaf;
    if (leaf.empty())
        return base;
    std::string joined = base;
    if (joined[joined.size() - 1] != '/')
        joined += "/";
    std::string clean_leaf = leaf;
    while (!clean_leaf.empty() && clean_leaf[0] == '/')
        clean_leaf.erase(0, 1);
    while (!clean_leaf.empty() && clean_leaf[clean_leaf.size() - 1] == '/')
        clean_leaf.erase(clean_leaf.size() - 1);
    return joined + clean_leaf;
}

bool hasPathTraversal(const std::string& value)
{
    std::string path = stripQueryCopy(value);
    std::string::size_type pos = 0;
    while (pos < path.size()) {
        while (pos < path.size() && path[pos] == '/')
            ++pos;
        std::string::size_type end = pos;
        while (end < path.size() && path[end] != '/')
            ++end;
        if (end - pos == 2 && path[pos] == '.' && path[pos + 1] == '.')
            return true;
        pos = end;
    }
    return false;
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
