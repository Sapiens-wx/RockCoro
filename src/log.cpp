#if DEBUG
#include "log.h"

#include <cstdio>
#include <thread>
#include <mutex>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <atomic>

namespace rockcoro{

namespace fs = std::filesystem;

// 每个线程独有的 FILE*
thread_local FILE* tlog_file = nullptr;
std::atomic<int> _tid{0};

// 工具函数：生成文件路径
static std::string get_thread_log_path() {
	    std::ostringstream oss;
		int tmp=_tid.fetch_add(1);
		    oss << "./thread_logs/" << tmp << ".log";
			    return oss.str();
}

void init_thread_logger() {
	if (tlog_file == nullptr) {
		std::string path = get_thread_log_path();
		tlog_file = std::fopen(path.c_str(), "w");
		if (!tlog_file) {
			std::perror("Failed to open thread log file");
		}
		std::cout<<"open file "<<path<<'\n';
	}
}

void logf(const char* fmt, ...) {
	    if (tlog_file == nullptr) {
			        init_thread_logger();
					    }
						    if (!tlog_file) return;

							    char buffer[4096];

								    va_list args;
									    va_start(args, fmt);
										    int n = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
											    va_end(args);

												    if (n > 0) {
														        std::fwrite(buffer, 1, n, tlog_file);
																        std::fflush(tlog_file);
																		    }
}

void close_thread_logger() {
	if (tlog_file) {
		std::fclose(tlog_file);
		tlog_file = nullptr;
	}
}

}
#endif
