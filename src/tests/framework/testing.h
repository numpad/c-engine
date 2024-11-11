#ifndef CENGINE_TESTING_H
#define CENGINE_TESTING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TESTS 256
typedef int (*test_fn)(void*);

struct testcase {
	const char *name;
	test_fn run;

	// Error Info
	char *error_file;
	int error_line_nr;
	char *error_assertion;
	int error_assert_nr;
};

#define TEST_SUCCESS do { return 0; } while(0)
#define TEST_FAILURE do { return 1; } while(0)

#define TEST_ASSERT(cond)                                   \
	do {                                                    \
		struct testcase *ctx = (struct testcase *)_test_context;  \
		++ctx->error_assert_nr;                             \
		if (!(cond)) {                                      \
			ctx->error_file = __FILE__;                     \
			ctx->error_line_nr = __LINE__;                  \
			ctx->error_assertion = #cond;                   \
			return 1;                                       \
		}                                                   \
	} while(0)
#define TEST_ASSERT_STR(expected, actual)                             \
	do {                                                              \
		struct testcase *ctx = (struct testcase *)_test_context;      \
		++ctx->error_assert_nr;                                       \
		if (strcmp(expected, actual) != 0) {                          \
			ctx->error_file = __FILE__;                               \
			ctx->error_line_nr = __LINE__;                            \
			ctx->error_assertion = malloc(256 * sizeof(char));        \
			sprintf(ctx->error_assertion, "Expected: \"%s\",\n        got: \"%s\"", expected, actual); \
			return 1;                                                 \
		}                                                             \
	} while(0)

extern struct testcase tests[MAX_TESTS];
extern int TEST_COUNT;

#define TEST(TESTNAME)                            \
	int test_run_##TESTNAME(void *);              \
	__attribute__((constructor))                  \
	void test_register_##TESTNAME(void) {         \
		tests[TEST_COUNT++] = (struct testcase) { \
			.run = test_run_##TESTNAME,           \
			.name = #TESTNAME,                    \
			.error_file = NULL,                   \
			.error_line_nr = -1,                  \
			.error_assertion = NULL,              \
		};                                        \
	}                                             \
	int test_run_##TESTNAME(void *_test_context)

int testing_run_all(void);

#endif

