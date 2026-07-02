#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

namespace Utils {

std::string toLowerCopy(const std::string& value);
std::string trimCopy(const std::string& value);
std::string stripQueryCopy(const std::string& value);
std::string normalizePathCopy(const std::string& value);
std::string joinPathCopy(const std::string& base, const std::string& leaf);
bool hasPathTraversal(const std::string& value);
bool makeNonBlocking(int fd);
void closeFdSafe(int fd);

}

#endif
