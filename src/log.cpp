#include "log.h"
#if DEBUG

#include <stdio.h>
#include <atomic>
#include <pthread.h>

namespace rockcoro
{

	static std::atomic<int> _tid{0};

	struct Logger
	{
		FILE *log_file;
		pthread_mutex_t mutex_log;
		Logger()
		{
			const char path[] = "./log";
			log_file = std::fopen(path, "w");
			if (!log_file)
			{
				printf("ERROR: Failed to open thread log file");
			}
			// init mutex
			pthread_mutex_init(&mutex_log, nullptr);
		}
		~Logger()
		{
			if (log_file)
			{
				fclose(log_file);
			}
			pthread_mutex_destroy(&mutex_log);
		}
	};

	struct LoggerTL
	{
		int id;
		LoggerTL()
		{
			id = _tid.fetch_add(1);
		}
	};

	static Logger logger;
	static thread_local LoggerTL logger_tl;

	void logf(const char *fmt, ...)
	{
		char buffer[4096];

		// print out the thread id first
		int id_length = snprintf(buffer, sizeof(buffer), "[%d] ", logger_tl.id);

		va_list args;
		va_start(args, fmt);
		int n = vsnprintf(buffer + id_length, sizeof(buffer) - id_length, fmt, args) + id_length;
		va_end(args);

		if (n > 0)
		{
			pthread_mutex_lock(&logger.mutex_log);
			fwrite(buffer, 1, n, logger.log_file);
			printf(buffer);
			fflush(logger.log_file);
			pthread_mutex_unlock(&logger.mutex_log);
		}
	}

}
#endif
