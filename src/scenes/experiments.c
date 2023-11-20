#include "experiments.h"

#include <stdlib.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <cglm/cglm.h>
#include "engine.h"

struct player_s;

typedef struct ship_s {
	vec2 pos;
	vec2 vel;
	vec2 size;
	float angle;
	
	struct player_s *captain;
} ship_t;

typedef struct planet_s {
	vec2 pos;
	float radius;
} planet_t;

typedef struct player_s {
	vec2 pos;
	vec2 vel;
	float radius;

	planet_t *on_planet;
	ship_t *ship;
} player_t;

// vars
static ship_t ship;
static const int planets_max = 1;
static planet_t *planets = NULL;
static player_t pl;

static void scene_experiments_load(struct scene_experiments_s *scene, struct engine_s *engine) {
	// planets
	assert(planets == NULL);
	planets = malloc(sizeof(planet_t) * planets_max);

	for (int i = 0; i < planets_max; ++i) {
		planets[i].pos[0] = 0.0;
		planets[i].pos[1] = 0.0f;
		planets[i].radius = 200.0f;
	}

	// ship
	ship.pos[0] = planets[0].pos[0] - 50.0f;
	ship.pos[1] = planets[0].pos[1] - planets[0].radius - 20.0f;
	ship.size[0] = 30.0f;
	ship.size[1] = 50.0f;
	ship.angle = glm_rad(-11.0f);
	ship.captain = NULL;

	// player
	pl.pos[0] = planets[0].pos[0] + 50.0f;
	pl.pos[1] = planets[0].pos[1] - planets[0].radius;
	glm_vec2_zero(pl.vel);
	pl.radius = 11.0f;
	pl.on_planet = NULL;
	pl.ship = NULL;


}

static void scene_experiments_destroy(struct scene_experiments_s *scene, struct engine_s *engine) {
	free(planets);
	planets = NULL;
}

static void scene_experiments_update(struct scene_experiments_s *scene, struct engine_s *engine, float dt) {
	if (pl.ship) {
		glm_vec2_add(pl.ship->pos, pl.ship->vel, pl.ship->pos);
		glm_vec2_scale(pl.ship->vel, 0.9999f, pl.ship->vel);
	
		int mx = 0, my = 0;
		if (engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
			mx = (engine->input_drag.x < engine->window_width * 0.5f) ? -1 : 1;
			my = (engine->input_drag.y < engine->window_height * 0.5f) ? -1 : 1;
		}

		if (my == -1) {
			const float thrust = 0.1f;
			pl.ship->vel[0] += cosf(pl.ship->angle - glm_rad(90)) * thrust;
			pl.ship->vel[1] += sinf(pl.ship->angle - glm_rad(90)) * thrust;
		}
		if (my == 1) {
			if (mx == -1) {
				pl.ship->angle -= glm_rad(4);
			}
			if (mx == 1) {
				pl.ship->angle += glm_rad(4);
			}
		}
	} else {
		// update physics
		glm_vec2_add(pl.pos, pl.vel, pl.pos);
		glm_vec2_scale(pl.vel, 0.9f, pl.vel);

		// gravity
		for (int i = 0; i < planets_max; ++i) {
			planet_t *p = &planets[i];
			
			vec2 pl_to_p;
			glm_vec2_sub(p->pos, pl.pos, pl_to_p);
			const float dist = glm_vec2_norm(pl_to_p);

			if (dist < p->radius + pl.radius) {
				pl.on_planet = p;
				
				vec2 p_to_pl;
				glm_vec2(pl_to_p, p_to_pl);
				glm_vec2_scale_as(p_to_pl, dist - (p->radius + pl.radius), p_to_pl);

				glm_vec2_add(pl.pos, p_to_pl, pl.pos);
			} else if (dist > p->radius + pl.radius) {
				pl.on_planet = NULL;

				glm_vec2_normalize(pl_to_p);
				glm_vec2_add(pl.vel, pl_to_p, pl.vel);
			}
			
		}

		// player input/movement
		if (engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
			if (engine->input_drag.y > engine->window_height * 0.6f && pl.on_planet) {
				float dir = 1.0f;
				if (engine->input_drag.x < engine->window_width * 0.5f) {
					dir = -1.0f;
				}

				vec2 up;
				glm_vec2_sub(pl.pos, pl.on_planet->pos, up);
				glm_vec2_normalize(up);
				vec2 right;
				right[0] = -up[1];
				right[1] = up[0];

				glm_vec2_scale(right, dir, right);
				glm_vec2_add(pl.vel, right, pl.vel);
			}
		}

		if (engine->input_drag.state == INPUT_DRAG_BEGIN) {
			if (engine->input_drag.y < engine->window_height * 0.6f && pl.on_planet) {
				vec2 up;
				glm_vec2_sub(pl.pos, pl.on_planet->pos, up);
				glm_vec2_scale_as(up, 10.0f, up); // jump height
				
				glm_vec2_add(pl.vel, up, pl.vel);
			}
		}
	}
}

static void scene_experiments_draw(struct scene_experiments_s *scene, struct engine_s *engine) {

	struct nk_context *nk = engine->nk;
	if (nk_begin(nk, "Stats", nk_rect(engine->window_width - 133.0f, 3.0f, 130.0f, 150.0f), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(nk, 13.0f, 1);
		nk_label_colored(nk, "// stats", NK_TEXT_LEFT, nk_rgb(99, 99, 99));

		nk_layout_row_dynamic(nk, 13.0f, 1);
		nk_labelf(nk, NK_TEXT_LEFT, "vel: %.2fm/s", glm_vec2_norm(pl.vel));

		nk_layout_row_dynamic(nk, 13.0f, 2);
		nk_labelf(nk, NK_TEXT_LEFT, "planet:");
		if (pl.on_planet) {
			nk_label_colored(nk, "yes", NK_TEXT_LEFT, nk_rgb(50, 220, 50));
		} else {
			nk_label_colored(nk, "no", NK_TEXT_LEFT, nk_rgb(220, 50, 50));
		}

		if (ship.captain == &pl) {
			nk_layout_row_dynamic(nk, 13.0f, 1);
			nk_label_colored(nk, "ahoi cap'n!", NK_TEXT_LEFT, nk_rgb(50, 50, 220));
		}

		nk_layout_row_dynamic(nk, 34.0f, 1);
		if (pl.ship) {
			if (nk_button_label(nk, "leave")) {
				pl.pos[0] = pl.ship->pos[0];
				pl.pos[1] = pl.ship->pos[1];
				pl.vel[0] = pl.ship->vel[0];
				pl.vel[1] = pl.ship->vel[1];
				ship.captain = NULL;
				pl.ship = NULL;
			}
		} else {
			const float ship_enter_distance = 50.0f;
			vec2 pl_ship;
			glm_vec2_sub(ship.pos, pl.pos, pl_ship);
			const float pl_ship_dist = glm_vec2_norm2(pl_ship);
			if (pl_ship_dist <= ship_enter_distance * ship_enter_distance) {
				if (nk_button_label(nk, "enter ship")) {
					ship.captain = &pl;
					pl.ship = &ship;
				}
			}
		}
	}
	nk_end(nk);


	NVGcontext *vg = engine->vg;
	if (pl.ship) {
		nvgTranslate(vg, -pl.ship->pos[0] + engine->window_width * 0.5f, -pl.ship->pos[1] + engine->window_height * 0.5f);
	} else {
		nvgTranslate(vg, -pl.pos[0] + engine->window_width * 0.5f, -pl.pos[1] + engine->window_height * 0.5f);
	}

	// ship
	nvgStrokeColor(vg, nvgRGB(255, 255, 255));
	nvgStrokeWidth(vg, 3.0f);

	nvgBeginPath(vg);
	nvgTranslate(vg, ship.pos[0], ship.pos[1]);
	nvgRotate(vg, ship.angle);

	nvgMoveTo(vg, 0, -ship.size[1] * 0.5f);
	nvgLineTo(vg, -ship.size[0] * 0.5f, ship.size[1] * 0.5f);

	nvgMoveTo(vg, 0, -ship.size[1] * 0.5f);
	nvgLineTo(vg, ship.size[0] * 0.5f, 0 + ship.size[1] * 0.5f);

	nvgMoveTo(vg, -ship.size[0] * 0.5f, ship.size[1] * 0.5f);
	nvgLineTo(vg, ship.size[0] * 0.5f, ship.size[1] * 0.5f);
	nvgStroke(vg);

	nvgRotate(vg, -ship.angle);
	nvgTranslate(vg, -ship.pos[0], -ship.pos[1]);

	// player
	if (!pl.ship) {
		nvgStrokeWidth(vg, 3.0f);
		nvgBeginPath(vg);
		nvgCircle(vg, pl.pos[0], pl.pos[1], pl.radius);
		nvgMoveTo(vg, pl.pos[0] + pl.radius, pl.pos[1] - 3.0f);
		nvgLineTo(vg, pl.pos[0] - pl.radius - 5.0f, pl.pos[1] - 3.0f);
		nvgStroke(vg);
	}

	// planets
	nvgStrokeWidth(vg, 4.0f);
	for (int i = 0; i < planets_max; ++i) {
		nvgBeginPath(vg);
		nvgCircle(vg, planets[i].pos[0], planets[i].pos[1], planets[i].radius);
		nvgStroke(vg);
	}

}

void scene_experiments_init(struct scene_experiments_s *scene, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene, engine);
	
	// init function pointers
	scene->base.load = (scene_load_fn)scene_experiments_load;
	scene->base.destroy = (scene_destroy_fn)scene_experiments_destroy;
	scene->base.update = (scene_update_fn)scene_experiments_update;
	scene->base.draw = (scene_draw_fn)scene_experiments_draw;
}

