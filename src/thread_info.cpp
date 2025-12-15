#include "thread_info.h"
#include <atomic>

namespace rockcoro {

static std::atomic<int> tid_counter = {0};

ThreadInfo::ThreadInfo()
{
    id = tid_counter.fetch_add(1);
}
} // namespace rockcoro