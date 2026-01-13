#pragma once
#include <cstdarg>
#include <pthread.h>
#include <string>

namespace rockcoro {

#define DEBUG 1
#if DEBUG

struct File;

struct Logger {
    static Logger inst;
    FILE *log_file;
    pthread_mutex_t mutex_log;

    Logger();
    ~Logger();
};

// log into different files for different threads
void logf(const char *fmt, ...);

#else
#define init_thread_logger()
#define logf(msg, ...)
#define close_thread_logger()
#endif

} // namespace rockcoro
