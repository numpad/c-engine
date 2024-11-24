#include "framework/testing.h"
#include "util/util.h"

TEST(ringbuffer) {
	RINGBUFFER(int, buffer, 4);

	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(0 == buffer.len);
	TEST_ASSERT(4 == buffer.capacity);

	memset(buffer.items, 0, sizeof(int) * 4);
	TEST_ASSERT(0 == buffer.items[0]);
	TEST_ASSERT(0 == buffer.items[1]);
	TEST_ASSERT(0 == buffer.items[2]);
	TEST_ASSERT(0 == buffer.items[3]);

	RINGBUFFER_APPEND(buffer, 10);
	TEST_ASSERT(10 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT( 0 == buffer.items[2]);
	TEST_ASSERT( 0 == buffer.items[3]);
	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(1 == buffer.len);

	RINGBUFFER_APPEND(buffer, 20);
	TEST_ASSERT(10 == buffer.items[0]);
	TEST_ASSERT(20 == buffer.items[1]);
	TEST_ASSERT( 0 == buffer.items[2]);
	TEST_ASSERT( 0 == buffer.items[3]);
	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(2 == buffer.len);

	RINGBUFFER_APPEND(buffer, 30);
	TEST_ASSERT(10 == buffer.items[0]);
	TEST_ASSERT(20 == buffer.items[1]);
	TEST_ASSERT(30 == buffer.items[2]);
	TEST_ASSERT( 0 == buffer.items[3]);
	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(3 == buffer.len);

	RINGBUFFER_APPEND(buffer, 40);
	TEST_ASSERT(10 == buffer.items[0]);
	TEST_ASSERT(20 == buffer.items[1]);
	TEST_ASSERT(30 == buffer.items[2]);
	TEST_ASSERT(40 == buffer.items[3]);
	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(4 == buffer.len);

	int result;
	result = RINGBUFFER_CONSUME(buffer);
	TEST_ASSERT(10 == result);
	TEST_ASSERT( 0 == buffer.items[0]);
	TEST_ASSERT(20 == buffer.items[1]);
	TEST_ASSERT(30 == buffer.items[2]);
	TEST_ASSERT(40 == buffer.items[3]);
	TEST_ASSERT(1 == buffer.head);
	TEST_ASSERT(3 == buffer.len);

	result = RINGBUFFER_CONSUME(buffer);
	TEST_ASSERT(20 == result);
	TEST_ASSERT( 0 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT(30 == buffer.items[2]);
	TEST_ASSERT(40 == buffer.items[3]);
	TEST_ASSERT(2 == buffer.head);
	TEST_ASSERT(2 == buffer.len);

	RINGBUFFER_APPEND(buffer, 99);
	TEST_ASSERT(99 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT(30 == buffer.items[2]);
	TEST_ASSERT(40 == buffer.items[3]);
	TEST_ASSERT(2 == buffer.head);
	TEST_ASSERT(3 == buffer.len);

	result = RINGBUFFER_CONSUME(buffer);
	TEST_ASSERT(30 == result);
	TEST_ASSERT(99 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT( 0 == buffer.items[2]);
	TEST_ASSERT(40 == buffer.items[3]);
	TEST_ASSERT(3 == buffer.head);
	TEST_ASSERT(2 == buffer.len);

	result = RINGBUFFER_CONSUME(buffer);
	TEST_ASSERT(40 == result);
	TEST_ASSERT(99 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT( 0 == buffer.items[2]);
	TEST_ASSERT( 0 == buffer.items[3]);
	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(1 == buffer.len);

	result = RINGBUFFER_CONSUME(buffer);
	TEST_ASSERT(99 == result);
	TEST_ASSERT( 0 == buffer.items[0]);
	TEST_ASSERT( 0 == buffer.items[1]);
	TEST_ASSERT( 0 == buffer.items[2]);
	TEST_ASSERT( 0 == buffer.items[3]);
	TEST_ASSERT(1 == buffer.head);
	TEST_ASSERT(0 == buffer.len);

	TEST_SUCCESS;
}

TEST(ringbuffer_many_reads) {
	RINGBUFFER(int, buffer, 4);

	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(0 == buffer.len);
	TEST_ASSERT(4 == buffer.capacity);

	memset(buffer.items, 0, sizeof(int) * 4);
	
	int read_count = 100;
	while (--read_count > 0) {
		int result = RINGBUFFER_CONSUME(buffer);
		TEST_ASSERT(0 == result);
	}

	TEST_SUCCESS;
}

TEST(ringbuffer_many_writes) {
	RINGBUFFER(int, buffer, 4);

	TEST_ASSERT(0 == buffer.head);
	TEST_ASSERT(0 == buffer.len);
	TEST_ASSERT(4 == buffer.capacity);

	memset(buffer.items, 0, sizeof(int) * 4);
	
	int read_count = 100;
	while (--read_count > 0) {
		RINGBUFFER_APPEND(buffer, 42);
	}

	TEST_SUCCESS;
}
