cmake_minimum_required(VERSION 3.16)
project(RockCoro LANGUAGES CXX)

# ============ 基本设置 ============
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)  # 我们不启用任何 C++ 标准库特性
set(CMAKE_CXX_EXTENSIONS OFF)

# ============ 禁用 C++ runtime 特性 ============
# 不启用RTTI，不注册C++析构，不使用libstdc++
add_compile_options(
    -fno-rtti
    -fno-use-cxa-atexit
)

# 禁止链接libstdc++（仅保留C标准库）
# -nostdlib++: 不链接C++标准库
# -nodefaultlibs: 移除默认链接的标准库 (需要我们手动加上 libc/m/atomic 等)
set(NOSTDLIB_FLAGS "-nostdlib++ -nodefaultlibs")

# ============ 构建核心库 ============
file(GLOB_RECURSE ROCKCORO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp
)

add_library(rockcoro STATIC ${ROCKCORO_SOURCES})

target_include_directories(rockcoro
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 指定C ABI友好编译参数
target_compile_options(rockcoro PRIVATE
    -Wall -Wextra -Werror
    -fno-rtti
    -fno-use-cxa-atexit
)

# 链接阶段：仅使用C库，不链接libstdc++
target_link_options(rockcoro PRIVATE
    ${NOSTDLIB_FLAGS}
    -lc -lm -lgcc
)

# ============ 测试与示例（可选） ============
option(ROCKCORO_BUILD_TESTS "Build unit tests" OFF)
option(ROCKCORO_BUILD_EXAMPLES "Build examples" OFF)

if(ROCKCORO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(ROCKCORO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============ 输出构建信息 ============
message(STATUS "=== RockCoro build summary ===")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER})")
message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "No stdlib++: ${NOSTDLIB_FLAGS}")
