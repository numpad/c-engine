#include "game.h"
#include <stdlib.h>
#include "scenes/scene.h"

static void game_load(struct game_s *menu, struct engine_s *engine) {
}

static void game_destroy(struct game_s *game, struct engine_s *engine) {
	free(game->world);

}

static void game_update(struct game_s *menu, struct engine_s *engine, float dt) {
}

static void game_draw(struct game_s *menu, struct engine_s *engine) {
}

void game_init(struct game_s *game, struct engine_s *engine, int w, int h) {
	scene_init((struct scene_s *)game, engine);

	game->base.load = (scene_load_fn)game_load;
	game->base.destroy = (scene_destroy_fn)game_destroy;
	game->base.update = (scene_update_fn)game_update;
	game->base.draw = (scene_draw_fn)game_draw;

	game->world_width = w;
	game->world_height = h;
	game->world = malloc(w * h * sizeof(unsigned char));

}

