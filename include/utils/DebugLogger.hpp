#ifndef DEBUGLOGGER_HPP
#define DEBUGLOGGER_HPP

#include <iostream>

class DebugLogger {
public:
    static void setEnabled(bool enabled);
    static bool isEnabled();
    static std::ostream& stream();

private:
    DebugLogger();
};

#define DEBUG_LOG \
    if (!DebugLogger::isEnabled()) { \
    } else \
        DebugLogger::stream()

#endif
