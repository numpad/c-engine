#ifndef GAME_H
#define GAME_H

typedef struct {
	unsigned char *world;
	int world_width, world_height;
} game_t;


int game_init(game_t *game, int w, int h);
int game_destroy(game_t *game);

#endif

