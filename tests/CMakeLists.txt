# 启用测试功能
enable_testing()

# 加入 GoogleTest 子目录（路径根据你的工程布局）
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/googletest)

# 定义测试可执行文件
add_executable(rockcoro_test
    test_main.cpp
    test_coroutine.cpp
)

# 链接 GoogleTest + 你的库
target_link_libraries(rockcoro_test
    PRIVATE
        rockcoro        # 你的核心库
        gtest_main      # GoogleTest 主入口
)

# 注册测试
include(GoogleTest)
gtest_discover_tests(rockcoro_test)