#include "testing.h"

#include <stdio.h>
#include <time.h>

struct testcase tests[256];
int TEST_COUNT = 0;

static double get_time_in_milliseconds(void);

int testing_run_all(void) {
	fprintf(stderr, "Running %d tests...\n", TEST_COUNT);
	fprintf(stderr, "-------------------------------------------\n");

	int failures = 0;
	double start_time_ms = get_time_in_milliseconds();
	
	for (int i = 0; i < TEST_COUNT; ++i) {
		fprintf(stderr, "(%d/%d) %27s : ", i + 1, TEST_COUNT, tests[i].name);
		const int test_result = tests[i].run(&tests[i]);
		if (test_result == 0) {
			fprintf(stderr, "\x1b[32mOK\x1b[0m\n");
		} else {
			fprintf(stderr, "\x1b[31mFailure\x1b[0m\n");
			++failures;
		}
	}

	double end_time_ms = get_time_in_milliseconds();
	
	fprintf(stderr, "-------------------------------------------\n");
	fprintf(stderr, "Failures:\n");
	for (int i = 0; i < TEST_COUNT; ++i) {
		if (tests[i].error_file != NULL) {
			fprintf(stderr, " * \x1b[4m%s:%d\x1b[0m\n", tests[i].error_file, tests[i].error_line_nr);
			fprintf(stderr, "   %s\n", tests[i].error_assertion);
		}
	}

	fprintf(stderr, "\nFinished in %.2g seconds.\n", (end_time_ms - start_time_ms));
	fprintf(stderr, "\x1b[%dm", (failures ? 31 : 32));
	fprintf(stderr, "Ran %d test%s, %d failures.\n",
		TEST_COUNT, (TEST_COUNT == 1 ? "" : "s"), failures);
	fprintf(stderr, "\x1b[0m");

	return (failures > 0);
}

double get_time_in_milliseconds(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

