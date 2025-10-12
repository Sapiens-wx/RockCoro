#include <gtest/gtest.h>
#include "basic_struct/data_structures.h"

TEST(FactorialTest, HandlesPositiveInput) {
    EXPECT_EQ(test_func(1), 2);
    EXPECT_EQ(test_func(2), 3);
    EXPECT_EQ(test_func(3), 4);
    EXPECT_EQ(test_func(8), 9);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}