#include "../../include/utils/DebugLogger.hpp"

static bool g_debug_enabled = false;

void DebugLogger::setEnabled(bool enabled) { g_debug_enabled = enabled; }

bool DebugLogger::isEnabled() { return g_debug_enabled; }

std::ostream& DebugLogger::stream() { return std::cout; }
