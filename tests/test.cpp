#include <gtest/gtest.h>

TEST(TrivialTest, SanityCheck) {
    EXPECT_EQ(1+1, 2);
}

TEST(TrivialTest, BooleanLogic) {
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}