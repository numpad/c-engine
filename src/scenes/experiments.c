#include "experiments.h"

#include <stdlib.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <cglm/cglm.h>
#include "engine.h"

struct player_s;
struct planet_s;
struct deco_s;

typedef struct ship_s {
	vec2 pos;
	vec2 vel;
	vec2 size;
	float angle;
	
	struct player_s *captain;
	struct planet_s *in_atmosphere;
} ship_t;

typedef struct planet_s {
	vec2 pos;
	float radius;
	float gravity_radius;
} planet_t;

typedef struct player_s {
	vec2 pos;
	vec2 vel;
	float radius;

	float walk_speed;

	planet_t *on_planet;
	planet_t *in_atmosphere;
	ship_t *nearby_ship;
	ship_t *ship;
} player_t;

typedef struct deco_s {
	vec2 pos;
	float angle;
} deco_t;

// functions
static void player_move(struct engine_s *engine, player_t *player) {
	if (engine->input_drag.state == INPUT_DRAG_END || engine->input_drag.state == INPUT_DRAG_NONE) {
		return;
	}

	const float x = (engine->input_drag.x / engine->window_width)  * 2.0f - 1.0f;
	const float y = (engine->input_drag.y / engine->window_height) * 2.0f - 1.0f;
	
	if (player->ship == NULL && player->on_planet) {
		// left or right
		float dir = 0.0f;
		if (y > 0.4f) {
			if (x < 0.0f) {
				dir = -1.0f;
			} else if (x > 0.0f) {
				dir = 1.0f;
			}
		}

		if (dir != 0.0f) {
			// move parallel to surface
			vec2 to_planet_dir;
			glm_vec2_sub(player->on_planet->pos, player->pos, to_planet_dir);
			// TODO: this should rarely happen, just return early?
			assert(glm_vec2_norm(to_planet_dir) > 0.01f);
			glm_vec2_normalize(to_planet_dir);
			
			vec2 right;
			right[0] = to_planet_dir[1];
			right[1] = -to_planet_dir[0];
			glm_vec2_scale(right, dir * player->walk_speed, right);

			glm_vec2_add(right, to_planet_dir, right);

			glm_vec2_add(player->vel, right, player->vel);
		}
	}

	if (player->ship) {
		if (y < 0.0f) {
			const float thrust = 0.7f;
			player->ship->vel[0] += cosf(player->ship->angle - glm_rad(90)) * thrust;
			player->ship->vel[1] += sinf(player->ship->angle - glm_rad(90)) * thrust;
		}

		if (y > 0.4f) {
			float dir = 0.0f;
			if (x < 0.0f) {
				dir = -1.0f;
			} else if (x > 0.0f) {
				dir = 1.0f;
			}

			player->ship->angle += glm_rad(3) * dir;
		}

	}
}

static void planet_generate(planet_t *planet, vec2 pos) {
	planet->pos[0] = pos[0];
	planet->pos[1] = pos[1];
	planet->radius = 200.0f;
	planet->gravity_radius = planet->radius * 1.7f;
}

static void planet_collide(planet_t *planet, vec2 pos, vec2 vel, float radius, planet_t **on_planet, planet_t **in_atmosphere) {
	vec2 to_planet; // planet ← pos
	glm_vec2_sub(planet->pos, pos, to_planet);
	vec2 to_planet_dir; // planet ←
	glm_vec2_normalize_to(to_planet, to_planet_dir);

	vec2 away; // planet →
	glm_vec2_negate_to(to_planet, away);
	glm_vec2_normalize(away);

	const float d = glm_vec2_norm(to_planet);
	if (in_atmosphere != NULL && d < planet->gravity_radius) {
		*in_atmosphere = planet;
	}

	if (d < planet->radius + radius) {
		if (on_planet != NULL) {
			*on_planet = planet;
		}

		const float dist_to_surface = (planet->radius + radius) - d;

		// move away
		vec2 to_surface;
		glm_vec2_scale(away, dist_to_surface, to_surface);
		glm_vec2_add(pos, to_surface, pos);

		// cancel vertical velocity
		if (glm_vec2_norm(vel) > 0.001f) {
			vec2 vel_dir;
			glm_vec2_normalize_to(vel, vel_dir);
			const float vertical_cancel = glm_vec2_norm(vel);
			const float dot = fmaxf(glm_vec2_dot(vel_dir, to_planet_dir), 0.0f);

			vec2 surface_vel;
			glm_vec2_scale_as(to_surface, vertical_cancel * dot * 0.5f, surface_vel);
			glm_vec2_add(vel, surface_vel, vel);
		}
	}
}

static void planet_attract(planet_t *planet, vec2 pos, vec2 vel) {
	vec2 to_planet; // planet ← pos
	glm_vec2_sub(planet->pos, pos, to_planet);

	const float d = glm_vec2_norm(to_planet);
	if (d > planet->radius && d <= planet->gravity_radius) {
		const float dpercent = 1.0f - (d / planet->gravity_radius);

		glm_vec2_scale_as(to_planet, dpercent, to_planet);
		glm_vec2_add(vel, to_planet, vel);
	}
}

// vars
static const int planets_max = 12;
static planet_t *planets = NULL;
static const int ships_max = 4;
static ship_t *ships;
static player_t pl;
static const int decos_per_planet = 6;
static deco_t *decos;
static float minimap_scale = 1.0f / 100.0f;

static const float ship_enter_distance = 50.0f;

// scene
static void scene_experiments_load(struct scene_experiments_s *scene, struct engine_s *engine) {
	// set background color
	engine_set_clear_color(0.01f, 0.01f, 0.02f);
	srandom(0);

	// planets
	assert(planets == NULL);
	planets = malloc(sizeof(planet_t) * planets_max);

	for (int i = 0; i < planets_max; ++i) {
		vec2 pos;
		pos[0] = random() % 14000 - 7000;
		pos[1] = random() % 14000 - 7000;
		planet_generate(&planets[i], pos);
	}
	
	// deco
	assert(decos == NULL);
	decos = malloc(sizeof(deco_t) * decos_per_planet * planets_max);

	// ships
	assert(ships == NULL);
	ships = malloc(sizeof(ship_t) * ships_max);
	for (int i = 0; i < ships_max; ++i) {
		vec2 offset;
		offset[0] = cosf((random() % 520));
		offset[1] = sinf((random() % 520));
		glm_vec2_scale_as(offset, planets[0].gravity_radius - (random() % 50), offset);

		ship_t *ship = &ships[i];
		ship->pos[0] = planets[0].pos[0] + offset[0];
		ship->pos[1] = planets[0].pos[1] + offset[1];
		glm_vec2_zero(ship->vel);
		ship->size[0] = 30.0f;
		ship->size[1] = 50.0f;
		ship->angle = glm_rad(-11.0f);
		ship->captain = NULL;
		ship->in_atmosphere = NULL;
	}

	// player
	pl.pos[0] = planets[0].pos[0] + 50.0f;
	pl.pos[1] = planets[0].pos[1] - planets[0].radius * 1.25f;
	glm_vec2_zero(pl.vel);
	pl.radius = 11.0f;
	pl.walk_speed = 0.45f;
	pl.on_planet = NULL;
	pl.in_atmosphere = NULL;
	pl.nearby_ship = NULL;
	pl.ship = NULL;

}

static void scene_experiments_destroy(struct scene_experiments_s *scene, struct engine_s *engine) {
	free(planets);
	planets = NULL;
	free(ships);
	ships = NULL;
	free(decos);
	decos = NULL;
}

static void scene_experiments_update(struct scene_experiments_s *scene, struct engine_s *engine, float dt) {
	// physics
	pl.on_planet = NULL;
	pl.in_atmosphere = NULL;
	for (int j = 0; j < ships_max; ++j) {
		ships[j].in_atmosphere = NULL;
	}
	for (int i = 0; i < planets_max; ++i) {
		planet_attract(&planets[i], pl.pos, pl.vel);
		planet_collide(&planets[i], pl.pos, pl.vel, pl.radius, &pl.on_planet, &pl.in_atmosphere);

		for (int j = 0; j < ships_max; ++j) {
			planet_attract(&planets[i], ships[j].pos, ships[j].vel);
			planet_collide(&planets[i], ships[j].pos, ships[j].vel, ships[j].size[0], NULL, &ships[j].in_atmosphere);
		}
	}

	player_move(engine, &pl);

	// update player position
	glm_vec2_add(pl.pos, pl.vel, pl.pos);
	if (pl.on_planet) {
		glm_vec2_scale(pl.vel, 0.9f, pl.vel);
	} else if (pl.in_atmosphere) {
		glm_vec2_scale(pl.vel, 0.9f, pl.vel);
	} else {
		glm_vec2_scale(pl.vel, 0.999f, pl.vel);
	}

	// update ship positions
	pl.nearby_ship = NULL;
	for (int i = 0; i < ships_max; ++i) {
		ship_t *ship = &ships[i];
		glm_vec2_add(ship->pos, ship->vel, ship->pos);
		if (ship->in_atmosphere) {
			glm_vec2_scale(ship->vel, 0.9f, ship->vel);
		} else {
			glm_vec2_scale(ship->vel, 0.999f, ship->vel);
		}

		// check if player is near
		const float dist_to_ship2 = glm_vec2_distance2(ship->pos, pl.pos);
		if (!pl.ship && pl.nearby_ship == NULL && dist_to_ship2 < ship_enter_distance * ship_enter_distance) {
			pl.nearby_ship = ship;
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
		} else if (pl.in_atmosphere) {
			nk_label_colored(nk, "close", NK_TEXT_LEFT, nk_rgb(220, 180, 50));
		} else {
			nk_label_colored(nk, "no", NK_TEXT_LEFT, nk_rgb(220, 50, 50));
		}

		if (pl.ship) {
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
				pl.ship->captain = NULL;
				pl.ship = NULL;
			}
		} else {
			if (pl.nearby_ship) {
				if (nk_button_label(nk, "enter ship")) {
					pl.nearby_ship->captain = &pl;
					pl.ship = pl.nearby_ship;
					pl.nearby_ship = NULL;
				}
			}
		}
	}
	nk_end(nk);


	NVGcontext *vg = engine->vg;
	nvgSave(vg);

	if (pl.ship) {
		nvgTranslate(vg, -pl.ship->pos[0] + engine->window_width * 0.5f, -pl.ship->pos[1] + engine->window_height * 0.5f);
	} else {
		nvgTranslate(vg, -pl.pos[0] + engine->window_width * 0.5f, -pl.pos[1] + engine->window_height * 0.5f);
	}

	// ship
	for (int i = 0; i < ships_max; ++i) {
		ship_t *ship = &ships[i];
		if (ship == pl.nearby_ship) {
			nvgStrokeColor(vg, nvgRGB(140, 140, 225));
		} else {
			nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		}
		nvgStrokeWidth(vg, 3.0f);

		nvgBeginPath(vg);
		nvgTranslate(vg, ship->pos[0], ship->pos[1]);
		nvgRotate(vg, ship->angle);

		nvgMoveTo(vg, 0, -ship->size[1] * 0.5f);
		nvgLineTo(vg, -ship->size[0] * 0.5f, ship->size[1] * 0.5f);

		nvgMoveTo(vg, 0, -ship->size[1] * 0.5f);
		nvgLineTo(vg, ship->size[0] * 0.5f, 0 + ship->size[1] * 0.5f);

		nvgMoveTo(vg, -ship->size[0] * 0.5f, ship->size[1] * 0.5f);
		nvgLineTo(vg, ship->size[0] * 0.5f, ship->size[1] * 0.5f);
		nvgStroke(vg);

		nvgRotate(vg, -ship->angle);
		nvgTranslate(vg, -ship->pos[0], -ship->pos[1]);
	}


	// player
	nvgStrokeColor(vg, nvgRGB(255, 255, 255));
	if (!pl.ship) {
		nvgStrokeWidth(vg, 3.0f);
		nvgBeginPath(vg);
		nvgCircle(vg, pl.pos[0], pl.pos[1], pl.radius);
		nvgMoveTo(vg, pl.pos[0] + pl.radius, pl.pos[1] - 3.0f);
		nvgLineTo(vg, pl.pos[0] - pl.radius - 5.0f, pl.pos[1] - 3.0f);
		nvgStroke(vg);
	}

	// planets
	for (int i = 0; i < planets_max; ++i) {
		nvgBeginPath(vg);
		nvgStrokeWidth(vg, 4.0f);
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgCircle(vg, planets[i].pos[0], planets[i].pos[1], planets[i].radius);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, nvgRGBA(200, 0, 0, 100));
		nvgCircle(vg, planets[i].pos[0], planets[i].pos[1], planets[i].gravity_radius);
		nvgStroke(vg);
	}

	nvgRestore(vg);

	// minimap
	{
		const float x = 100.0f, y = 100.0f;
		const float r = 90.0f;

		nvgStrokeWidth(vg, 1.0f);

		// planets
		nvgSave(vg);
		nvgStrokeColor(vg, nvgRGB(120, 120, 220));
		nvgBeginPath(vg);
		nvgTranslate(vg, x, y);
		nvgMoveTo(vg, -3.0f, 0.0f);
		nvgLineTo(vg, +3.0f, 0.0f);
		nvgMoveTo(vg, 0.0f, -3.0f);
		nvgLineTo(vg, 0.0f, +3.0f);
		nvgStroke(vg);
		nvgRestore(vg);

		for (int i = 0; i < planets_max; ++i) {
			const planet_t *p = &planets[i];
			nvgSave(vg);
			nvgFillColor(vg, nvgRGB(220, 220, 220));
			nvgBeginPath(vg);
			nvgTranslate(vg, x + p->pos[0] * minimap_scale, y + p->pos[1] * minimap_scale);
			nvgCircle(vg, 0.0f, 0.0f, p->radius * minimap_scale);
			nvgFill(vg);
			nvgRestore(vg);
		}

		// player
		vec2 p;
		if (pl.ship) {
			glm_vec2_copy(pl.ship->pos, p);
		} else {
			glm_vec2_copy(pl.pos, p);
		}

		nvgSave(vg);
		nvgBeginPath(vg);
		nvgTranslate(vg, x + p[0] * minimap_scale, y + p[1] * minimap_scale);
		nvgStrokeColor(vg, nvgRGB(0, 220, 0));
		nvgRect(vg, -2.0f, -2.0f, 4.0f, 4.0f);
		nvgStroke(vg);
		nvgRestore(vg);

		// border
		nvgBeginPath(vg);
		nvgStrokeWidth(vg, 2.0f);
		nvgStrokeColor(vg, nvgRGBA(0, 0, 180, 255));
		nvgCircle(vg, x, y, r);
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, nvgRGBA(160, 160, 180, 255));
		nvgCircle(vg, x, y, r);
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

