#include <atomic>
#include <gtest/gtest.h>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include "bootstrap.h"
#include "basic_struct/data_structures.h"
#include "log.h"


using namespace rockcoro;

int main(int argc, char **argv)
{
	rockcoro::init();
    testing::InitGoogleTest(&argc, argv);
	int run_result= RUN_ALL_TESTS();
	rockcoro::destroy();
	return run_result;
}
