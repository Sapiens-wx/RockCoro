# 启用测试功能
enable_testing()

file(GLOB_RECURSE TEST_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# 定义测试可执行文件
add_executable(rockcoro_test
    ${TEST_SOURCES}
)

# Includes
target_include_directories(rockcoro_test
	PRIVATE
		${CMAKE_SOURCE_DIR}/include   # 公共路径，依赖它的目标也会继承
		${CMAKE_SOURCE_DIR}/src       # 仅当前目标使用
)

# 链接 GoogleTest + 你的库
target_link_libraries(rockcoro_test
    PRIVATE
        rockcoro        # 你的核心库
        gtest_main      # GoogleTest 主入口
)

# asan and coverage
if(ROCKCORO_BUILD_DEBUG)
	target_compile_options(rockcoro_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_options(rockcoro_test PRIVATE -fsanitize=address)
endif()

# 注册测试
include(GoogleTest)
gtest_discover_tests(rockcoro_test)
