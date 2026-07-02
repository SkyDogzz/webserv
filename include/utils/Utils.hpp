#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

namespace Utils {

std::string toLowerCopy(const std::string& value);
std::string trimCopy(const std::string& value);
bool makeNonBlocking(int fd);
void closeFdSafe(int fd);

}

#endif
