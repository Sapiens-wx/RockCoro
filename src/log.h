#pragma once
#include <cstdarg>
#include <string>

namespace rockcoro{

#define DEBUG 1
#if DEBUG

// log into different files for different threads
void logf(const char* fmt, ...);

#else
#define init_thread_logger()
#define logf(msg, ...)
#define close_thread_logger()
#endif

}
