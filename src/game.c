#include "game.h"

#include <stdlib.h>

int game_init(game_t *game, int w, int h) {
	game->world_width = w;
	game->world_height = h;
	game->world = malloc(w * h * sizeof(unsigned char));

	return 0;
}

int game_destroy(game_t *game) {
	free(game->world);

	return 0;
}

