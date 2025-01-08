#include "framework/testing.h"

#include "util/util.h"

struct edge {
	float weight;
	struct node *to;
};

struct node {
	float x, y;
	usize edges_count;
	struct edge *edges;
	char _id;
};

TEST(pathfinding) {
	const int map_width = 4,
	          map_size = map_width * map_width,
	          edges_per_node = 4;
	struct node nodes[map_size];
	struct edge edges[map_size * edges_per_node];
	for (int y = 0; y < map_width; ++y) {
		for (int x = 0; x < map_width; ++x) {
			int i = x + y * map_width;
			int edges_start = i * edges_per_node;
			int edges_count = 0;
			if (y   > 0)         edges[edges_start + edges_count++] = (struct edge){ .weight=1.0f, .to=&nodes[x + (y-1) * map_width] };
			if (y+1 < map_width) edges[edges_start + edges_count++] = (struct edge){ .weight=1.0f, .to=&nodes[x + (y+1) * map_width] };
			if (x   > 0)         edges[edges_start + edges_count++] = (struct edge){ .weight=1.0f, .to=&nodes[(x-1) + y * map_width] };
			if (x+1 < map_width) edges[edges_start + edges_count++] = (struct edge){ .weight=1.0f, .to=&nodes[(x+1) + y * map_width] };
			nodes[i] = (struct node){
				.x=x, .y=y, .edges_count=edges_count, .edges=&edges[i * edges_per_node], ._id='A' + i };
		}
	}

	TEST_ASSERT('A' == nodes[0]._id);
	TEST_ASSERT('B' == nodes[1]._id);
	TEST_ASSERT('E' == nodes[4]._id);

	TEST_ASSERT(2 == nodes[0].edges_count);
	TEST_ASSERT('B' == nodes[0].edges[1].to->_id);
	TEST_ASSERT('E' == nodes[0].edges[0].to->_id);

	TEST_ASSERT(2 == nodes[0].edges_count);
	TEST_ASSERT(2 == nodes[3].edges_count);
	TEST_ASSERT('E' == nodes[0].edges[0].to->_id);
	TEST_ASSERT('B' == nodes[0].edges[1].to->_id);
	TEST_ASSERT('H' == nodes[3].edges[0].to->_id);
	TEST_ASSERT('C' == nodes[3].edges[1].to->_id);

	// test pathfinder
	const usize NONE = (usize)-2;
	const usize EMPTY = (usize)-1;
	usize start = 0;
	usize goal = 3;

	RINGBUFFER(usize, frontier, map_size);
	RINGBUFFER_APPEND(frontier, start);

	usize came_from[map_size];
	for (int i = 0; i < map_size; ++i)
		came_from[i] = EMPTY;
	came_from[start] = NONE;

	int NUMBER_OF_ITERATIONS = 0;
	while (frontier.len > 0) {
		usize current_node_i = RINGBUFFER_CONSUME(frontier);
		struct node *current_node = &nodes[current_node_i];

		// Early Exit
		if (current_node_i == goal) {
			break;
		}

		// Check all neighboring nodes.
		for (usize edge_i = 0; edge_i < current_node->edges_count; ++edge_i) {
			struct edge *next_edge = &current_node->edges[edge_i];
			struct node *next = next_edge->to;
			usize next_i = ((usize)next - (usize)&nodes[0]) / sizeof(*nodes);

			if (came_from[next_i] == EMPTY) {
				RINGBUFFER_APPEND(frontier, next_i);
				came_from[next_i] = current_node_i;
			}
		}

		TEST_ASSERT(NUMBER_OF_ITERATIONS++ < map_size);
	}

	TEST_ASSERT(2 == came_from[goal]);
	TEST_ASSERT(1 == came_from[2]);
	TEST_ASSERT(0 == came_from[1]);
	TEST_ASSERT(NONE == came_from[start]);
	
	TEST_SUCCESS;
}

