#include "log.h"
#if DEBUG

#include <pthread.h>
#include <stdio.h>

#include "thread_info.h"

namespace rockcoro {

Logger::Logger()
{
    const char path[] = "./log";
    log_file = std::fopen(path, "w");
    if (!log_file) {
        printf("ERROR: Failed to open thread log file");
    }
    // init mutex
    pthread_mutex_init(&mutex_log, nullptr);
}
Logger::~Logger()
{
    if (log_file) {
        fclose(log_file);
    }
    pthread_mutex_destroy(&mutex_log);
}

void logf(const char *fmt, ...)
{
    char buffer[4096];

    // print out the thread id first
    int id_length = snprintf(buffer, sizeof(buffer), "[%d] ", ThreadInfo::inst.id);

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buffer + id_length, sizeof(buffer) - id_length, fmt, args) + id_length;
    va_end(args);

    if (n > 0) {
        pthread_mutex_lock(&Logger::inst.mutex_log);
        fwrite(buffer, 1, n, Logger::inst.log_file);
        printf("%s", buffer);
        pthread_mutex_unlock(&Logger::inst.mutex_log);
    }
}

} // namespace rockcoro
#endif
