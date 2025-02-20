#include "brickbreaker.h"

#include <assert.h>
#include <box2d/box2d.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <flecs.h>
#include <nanovg.h>
#include "engine.h"
#include "util/util.h"


// Defines
#define P2d(a) (a).x, (a).y

#define ASTEROIDS_MAX 512
#define PARTICLES_MAX 1024
#define BULLETS_MAX 1024

// Components

typedef struct {
	vec2s pos;
	vec2s vel;
} c_object;
ECS_COMPONENT_DECLARE(c_object);

// Structs & Enums

enum game_state {
	GS_SHOP,
	GS_FLYING,
	GS_MAX,
};
const char *game_state_names[GS_MAX + 1] = {
	"GS_SHOP",
	"GS_FLYING",
	"Unknown State?!"
};

enum particletype {
	PT_RECT,
	PT_CHAR,
};

enum game_state game_state_next(enum game_state current_state) {
	switch (current_state) {
	case GS_SHOP      : return GS_FLYING;
	case GS_FLYING    : return GS_FLYING;
	case GS_MAX:
		break;
	};
	return GS_MAX;
}

typedef struct {
	vec2s p;
	vec2s vel;
	float radius;
	float ridges[8];
} asteroid_t;

typedef struct {
	float x;
	float w, h;
} paddle_t;

typedef struct {
	vec2s pos;
	vec2s target;
	float radius;
} bullet_t;

typedef struct {
	vec2s pos;
	vec2s vel;
	vec2s acc;
	float angle;
} ship_t;

typedef struct {
	enum particletype type;
	vec2s p, v;
	char chr;
	float lifetime;
	NVGcolor color;
} particle_t;

typedef struct {
	vec2s pos;
	vec2s vel;
	vec2s acc;
} player_t;


// State

static NVGcontext *vg;
static struct engine *g_engine;
static vec2s wsize;
static vec2s stick_origin;

static double g_time_elapsed;

NVGcolor g_foreground = { .rgba = {0.3f, 0.63f, 0.86f, 1.0f} };

struct Globals {
	enum game_state state;

	ship_t ship;
	player_t player;
	int money;

	asteroid_t asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	particle_t particles[PARTICLES_MAX];
	int particles_count;
} G;

// Functions

particle_t *particle_spawn(float x, float y) {
	//assert(G.particles_count < PARTICLES_MAX);
	if (G.particles_count >= PARTICLES_MAX) return NULL;

	particle_t *par = &G.particles[G.particles_count];
	par->p = (vec2s){ .x=x, .y=y };
	par->v = (vec2s){ .x=0.0f, .y=0.0f };
	par->type = PT_RECT;
	par->chr = '\0';
	par->color = nvgRGB(255, 255, 255);
	par->lifetime = 2.0f;

	++G.particles_count;
	return par;
}

particle_t *particle_spray(vec2s pos, float angle, float angle_variance) {
	particle_t *par = particle_spawn(P2d(pos));
	// assert(par != NULL);
	if (par == NULL) return NULL;

	float r = ((float)rand() / RAND_MAX) - 0.5f;
	angle = angle + (r * angle_variance);

	par->v = (vec2s){ .x=cosf(angle), .y=sinf(angle) };
	par->lifetime = 0.3f;

	return par;
}


void particle_remove(int i) {
	assert(i >= 0 && i < G.particles_count);

	if (G.particles_count > 0) {
		G.particles[i] = G.particles[G.particles_count - 1];
		--G.particles_count;
	}
}

asteroid_t *asteroid_spawn(float x, float y) {
	assert(G.asteroids_count < ASTEROIDS_MAX);

	asteroid_t *as = &G.asteroids[G.asteroids_count];
	as->p = (vec2s){ .x=x, .y=y };
	as->vel = (vec2s){ .x=0.0f, .y=0.0f };
	as->radius = 30.0f;
	int i = rand() % 5;
	if (i == 0) {
		as->ridges[0] = 25.0f;
		as->ridges[1] = 27.5f;
		as->ridges[2] = 30.0f;
		as->ridges[3] = 32.5f;
		as->ridges[4] = 35.0f;
		as->ridges[5] = 37.5f;
		as->ridges[6] = 40.0f;
		as->ridges[7] = 42.5f;
	} else if (i == 1) {
		as->ridges[0] = 35.0f;
		as->ridges[1] = 27.5f;
		as->ridges[2] = 38.0f;
		as->ridges[3] = 32.5f;
		as->ridges[4] = 30.0f;
		as->ridges[5] = 29.5f;
		as->ridges[6] = 44.0f;
		as->ridges[7] = 42.5f;
	} else {
		for (int j = 0; j < 8; ++j) {
			as->ridges[j] = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 20.0f + 30.0f;
		}
	}
	++G.asteroids_count;

	return as;
}

void asteroid_draw(asteroid_t *as) {
	nvgStrokeWidth(vg, 2.0f);
	nvgBeginPath(vg);
	nvgMoveTo(vg, as->p.x + cosf(0.0f) * as->ridges[7], as->p.y + sinf(0.0f) * as->ridges[7]);
	for (int i = 0; i < 8; ++i) {
		float angle = glm_rad((i / 8.0f) * 360.0f);
		nvgLineTo(vg, as->p.x + cosf(angle) * as->ridges[i], as->p.y + sinf(angle) * as->ridges[i]);
	}
	nvgClosePath(vg);
	nvgFillColor(vg, nvgRGBf(0.16f, 0.16f, 0.16f));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
	nvgStroke(vg);
}


void player_draw(void) {
	nvgBeginPath(vg);
	nvgCircle(vg, P2d(G.player.pos), 17.0f);
	nvgStrokeWidth(vg, 2.0f);
	nvgFillColor(vg, nvgRGB(210, 210, 210));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGB(180, 180, 180));
	nvgStroke(vg);
	
	nvgGlobalCompositeOperation(vg, NVG_DESTINATION_OVER);

	nvgBeginPath(vg);
	nvgRect(vg, G.player.pos.x - 10.0f, G.player.pos.y, 30.0f, 30.0f);
	nvgFillColor(vg, nvgRGB(0, 120, 240));
	nvgFill(vg);

	nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
}

// Helpers

void draw_background_stars(void) {
	struct rng_state state;
	rng_save_state(&state);

	rng_seed(123);

	nvgBeginPath(vg);
	for (int i = 0; i < 200; ++i) {
		nvgRect(vg, rng_f() * wsize.x, rng_f() * wsize.y, 4.0f, 4.0f);
	}
	nvgFillColor(vg, nvgRGBA(120, 120, 120, 20));
	nvgFill(vg);
	rng_restore_state(&state);
}


// Game States

void update_gs_shop(float dt) {
}

void draw_gs_shop(void) {
}

static inline vec2s relative_to_ship(float x, float y) {
	float a = G.ship.angle - glm_rad(90.0f);
	vec2s offset = (vec2s){
		.x=cosf(a) * x - sinf(a) * y,
		.y=sinf(a) * x + cosf(a) * y,
	};
	return glms_vec2_add(G.ship.pos, offset);
}

void update_gs_flying(float dt) {
	/*
	// Ship Controls
	int mouse_down = g_engine->input_drag.state == INPUT_DRAG_BEGIN || g_engine->input_drag.state == INPUT_DRAG_IN_PROGRESS;
	int fly_left = g_engine->input_drag.x <= wsize.x * 0.5f;
	int fly_right = g_engine->input_drag.x > wsize.x * 0.5f;

	vec2s ship_thruster_p = relative_to_ship(0.0f + 50.0f * (rng_fnd() - 0.5f), -25.0f);

	if (mouse_down) {
		float delta_angle = 0.0f;
		if (fly_left) {
			delta_angle -= glm_rad(2.5f);
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));

		}
		if (fly_right) {
			delta_angle += glm_rad(2.5f);
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));
			particle_spray(ship_thruster_p, G.ship.angle + glm_rad(180.0f), glm_rad(180.0f));
		}

		G.ship.angle += delta_angle;
		G.ship.acc.x += cosf(G.ship.angle) * 0.04f;
		G.ship.acc.y += sinf(G.ship.angle) * 0.04f;
	}
	*/

	// Player Controls
	if (g_engine->input_drag.state == INPUT_DRAG_BEGIN) {
		stick_origin.x = g_engine->input_drag.begin_x;
		stick_origin.y = g_engine->input_drag.begin_y;
	}
	int mouse_down = INPUT_DRAG_IS_DOWN(g_engine->input_drag);
	vec2s drag = {
		.x=g_engine->input_drag.x - stick_origin.x,
		.y=g_engine->input_drag.y - stick_origin.y
	};

	G.player.acc = GLMS_VEC2_ZERO;
	const float MAX_ACCELERATION = 0.04f;
	if (mouse_down && glm_vec2_norm2(drag.raw) >= glm_pow2(24.0f)) {
		//
		vec2s overdrag = glms_vec2_scale_as(drag, glm_vec2_norm(drag.raw) - 24.0f);
		overdrag = glms_vec2_scale(overdrag, 0.2);
		glm_vec2_add(stick_origin.raw, overdrag.raw, stick_origin.raw);
		// set player acc
		glm_vec2_scale_as(drag.raw, MAX_ACCELERATION, drag.raw);
		glm_vec2_add(G.player.acc.raw, drag.raw, G.player.acc.raw);
	}
	glm_vec2_add(G.player.pos.raw, G.player.vel.raw, G.player.pos.raw);
	glm_vec2_add(G.player.vel.raw, G.player.acc.raw, G.player.vel.raw);
	G.player.acc = GLMS_VEC2_ZERO;
	
	// Ship Physics
	glm_vec2_add(G.ship.pos.raw, G.ship.vel.raw, G.ship.pos.raw);
	glm_vec2_add(G.ship.vel.raw, G.ship.acc.raw, G.ship.vel.raw);
	G.ship.acc = GLMS_VEC2_ZERO;
	// Asteroid Physics
	for (int i = 0; i < G.asteroids_count; ++i) {
		asteroid_t *as = &G.asteroids[i];
		glm_vec2_add(as->p.raw, as->vel.raw, as->p.raw);
	}
}

void draw_gs_flying(void) {
	float size = 50.0f;

	vec2s p_front = {
		.x = 0.0f,
		.y = -size * 0.5f,
	};
	vec2s p_left = {
		.x = -size * 0.33f,
		.y = size * 0.5f,
	};
	vec2s p_right = {
		.x = size * 0.33f,
		.y = size * 0.5f,
	};

	// Draw Ship
	nvgTranslate(vg, G.ship.pos.x, G.ship.pos.y);
	nvgRotate(vg, G.ship.angle + glm_rad(90.0f));
	nvgBeginPath(vg);
		nvgMoveTo(vg, p_front.x, p_front.y);
		nvgLineTo(vg, p_left.x, p_left.y);
		nvgLineTo(vg, p_right.x, p_right.y);
	nvgClosePath(vg);
	nvgFillColor(vg, nvgRGBf(0.16f, 0.16f, 0.16f));
	nvgFill(vg);
	nvgStrokeWidth(vg, 3.0f);
	nvgStrokeColor(vg, nvgRGB(200, 0, 0));
	nvgStroke(vg);
	nvgResetTransform(vg);

	// Draw Input
	int mouse_down = INPUT_DRAG_IS_DOWN(g_engine->input_drag);
	if (mouse_down) {
		vec2s drag = {
			.x=g_engine->input_drag.x - stick_origin.x,
			.y=g_engine->input_drag.y - stick_origin.y
		};
		nvgStrokeColor(vg, nvgRGBf(1, 1, 1));
		nvgStrokeWidth(vg, 1.0f);
		// Mouse Direction
		if (glm_vec2_norm2(drag.raw) >= glm_pow2(24.0f)) {
			nvgStrokeColor(vg, nvgRGBf(1, 0, 0));
		}
		nvgBeginPath(vg);
		nvgMoveTo(vg, stick_origin.x, stick_origin.y);
		nvgLineTo(vg, g_engine->input_drag.x, g_engine->input_drag.y);
		nvgStroke(vg);
		// Stick Origin
		nvgBeginPath(vg);
		nvgCircle(vg, stick_origin.x, stick_origin.y, 24.0f);
		nvgStroke(vg);
	}
}


// Callbacks

static void load(struct scene_brickbreaker_s *scene, struct engine *engine) {
	vg = engine->vg;
	g_engine = engine;
	wsize = (vec2s){ .x = engine->window_width, .y = engine->window_height };
	stick_origin = GLMS_VEC2_ZERO;

	G = (struct Globals) {
		.state = GS_FLYING,
		.ship={ .pos={ .x=wsize.x * 0.5f, .y=wsize.y * 0.5f}, .vel=GLMS_VEC2_ZERO_INIT, .acc=GLMS_VEC2_ZERO_INIT, .angle=((float)rand() / RAND_MAX) * 360.0f },
		.player={ .pos={ .x = 200.0f, .y=200.0f }, .vel={0} },
		.money=50,
		.asteroids_count=0,
		.particles_count = 0,
	};

	// Set up level
	for (int i = 0; i < 10; ++i) {
		float rx = (rand() / (double)RAND_MAX) * wsize.x;
		float ry = (rand() / (double)RAND_MAX) * wsize.y;
		switch (rand() % 4) {
		case 0: rx = 0.0f; break;
		case 1: ry = 0.0f; break;
		case 2: rx = wsize.x; break;
		case 3: ry = wsize.y; break;
		};
		asteroid_t *as = asteroid_spawn(rx, ry);
		vec2s to_player = glms_vec2_sub(G.ship.pos, as->p);
		glm_vec2_scale_as(to_player.raw, 0.1f, as->vel.raw);
	}


	g_time_elapsed = 0.0;
	engine_set_clear_color(0.16f, 0.16f, 0.16f);
}

static void destroy(struct scene_brickbreaker_s *scene, struct engine *engine) {
}

static void update(struct scene_brickbreaker_s *scene, struct engine *engine, float dt) {
	wsize = (vec2s){ .x = engine->window_width, .y = engine->window_height };

	const float FIXED_TIME_STEP = (1.0f / 60.0f);
	static float accumulator = 0.0f;
	accumulator += dt;

	while (accumulator >= FIXED_TIME_STEP) {
		switch (G.state) {
			case GS_SHOP      : update_gs_shop(FIXED_TIME_STEP); break;
			case GS_FLYING    : update_gs_flying(FIXED_TIME_STEP); break;
			case GS_MAX       : break;
		};
		accumulator -= FIXED_TIME_STEP;
	}

	// Update particles
	for (int i = 0; i < G.particles_count; ++i) {
		particle_t *p = &G.particles[i];
		p->lifetime -= dt;
		if (p->lifetime <= 0.0f) {
			particle_remove(i);
			--i;
		}

		glm_vec2_add(p->p.raw, p->v.raw, p->p.raw);
		glm_vec2_scale(p->v.raw, 0.97f, p->v.raw);
	}

	g_time_elapsed += dt;
}

static void draw(struct scene_brickbreaker_s *scene, struct engine *engine) {
	draw_background_stars();

	// Draw Asteroids
	for (int i = 0; i < G.asteroids_count; ++i) {
		asteroid_t *as = &G.asteroids[i];
		asteroid_draw(as);
	}

	// Draw Player
	player_draw();

	// Draw Particles
	for (int i = 0; i < G.particles_count; ++i) {
		particle_t *p = &G.particles[i];
		nvgFillColor(vg, p->color);
		nvgBeginPath(vg);
		nvgRect(vg, p->p.x, p->p.y, 4.0f, 4.0f);
		nvgFill(vg);
	}

	// Draw State
	switch (G.state) {
		case GS_SHOP      : draw_gs_shop(); break;
		case GS_FLYING    : draw_gs_flying(); break;
		case GS_MAX       : break;
	};

	// hud
	static char txt_money[16];
	snprintf(txt_money, 16, "Money: %d", G.money);

	nvgFillColor(vg, g_foreground);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
	nvgFontSize(vg, 15.0f);
	nvgText(vg, 10.0f, wsize.y - 130.0f, txt_money, NULL);
}

void scene_brickbreaker_init(struct scene_brickbreaker_s *scene, struct engine *engine) {
	scene_init((struct scene_s *)scene, engine);

	scene->base.load    = (scene_load_fn)load;
	scene->base.destroy = (scene_destroy_fn)destroy;
	scene->base.update  = (scene_update_fn)update;
	scene->base.draw    = (scene_draw_fn)draw;
}

