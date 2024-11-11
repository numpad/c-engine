#include "framework/testing.h"

#include "util/util.h"

TEST(dummy_math_works) {
	TEST_ASSERT(2 == 1+1);
	TEST_ASSERT(4 == 8/2);
	TEST_ASSERT(6 == 9-3);

	int feiner_sand = 49;
	TEST_ASSERT(feiner_sand == 7*7);

	TEST_SUCCESS;
}

TEST(nearest_pow2) {
	TEST_ASSERT(8 == nearest_pow2(8));
	TEST_ASSERT(32 == nearest_pow2(29));
	TEST_ASSERT(32 == nearest_pow2(31));
	TEST_ASSERT(64 == nearest_pow2(33));
	TEST_ASSERT(4096 == nearest_pow2(4096));
	
	TEST_SUCCESS;
}

