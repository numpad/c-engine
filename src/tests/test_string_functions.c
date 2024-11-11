#include "framework/testing.h"

#include "util/str.h"

#define MAX_LENGTH 128
static char output[MAX_LENGTH];

TEST(this_always_works) {
	TEST_SUCCESS;
}

TEST(str_path_replace_filename) {
	int result = -1;
	
	result = str_path_replace_filename("path/to/file.txt", "other.csv", MAX_LENGTH, output);
	TEST_ASSERT(result == 0);
	TEST_ASSERT_STR("path/to/other.csv", output);
	
	result = str_path_replace_filename("path/to/file.txt", "sub/path.txt", MAX_LENGTH, output);
	TEST_ASSERT(result == 0);
	TEST_ASSERT_STR("path/to/sub/path.txt", output);
	
	result = str_path_replace_filename("path/to/file.txt", "sub/path.txt", MAX_LENGTH, output);
	TEST_ASSERT(result == 0);
	TEST_ASSERT_STR("path/to/sub/path.txt", output);

	result = str_path_replace_filename("path/cutoff.txt", "../cutoff.txt", MAX_LENGTH, output);
	TEST_ASSERT(result == 0);
	TEST_ASSERT_STR("path/cutoff.txt", output);

	TEST_SUCCESS;
}

