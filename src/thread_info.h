#pragma once

namespace rockcoro {

struct ThreadInfo {
    static thread_local ThreadInfo inst;

    int id;
    ThreadInfo();
};
} // namespace rockcoro