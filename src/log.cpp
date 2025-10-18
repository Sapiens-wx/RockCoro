#include "log.h"
#if DEBUG

#include <stdio.h>
#include <atomic>

namespace rockcoro{

// helper function: get thread-local log file path
static void get_thread_log_path(char* buf, size_t buf_size) {
		int tmp=_tid.fetch_add(1);
		snprintf(buf, buf_size, "./thread_logs/%d.log", tmp);
}

struct LoggerTL{
	FILE* log_file;
	LoggerTL(){
		char path[256];
		get_thread_log_path(path, sizeof(path)-1);
		log_file = std::fopen(path, "w");
		if (!log_file) {
			printf("ERROR: Failed to open thread log file");
		}
	}
	~LoggerTL(){
		if(log_file)
			fclose(log_file);
	}
}

// 每个线程独有的 FILE*
thread_local LoggerTL logger;
std::atomic<int> _tid{0};

void logf(const char* fmt, ...) {
	char buffer[4096];

	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (n > 0) {
		fwrite(buffer, 1, n, logger.log_file);
	}
}

}
#endif
