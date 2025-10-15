#pragma once
#include <cstdarg>
#include <string>

namespace rockcoro{

#define DEBUG 1
#if DEBUG

// 初始化日志系统（可创建目录）
void init_thread_logger();

// 打印日志（参数风格与 printf 一致）
void logf(const char* fmt, ...);

// 可选：关闭当前线程的日志文件（例如在退出前调用）
void close_thread_logger();

#else
#define init_thread_logger()
#define logf(msg, ...)
#define close_thread_logger()
#endif

}
