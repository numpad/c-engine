#include "planes.h"

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <flecs.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stb_ds.h>
#include <stb_perlin.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/graphics2d.h"

//
// structs & enums
//

#define ZLAYER_GROUND       0.0f
#define ZLAYER_PEOPLE       0.1f
#define ZLAYER_SHADOW       0.2f
#define ZLAYER_ABOVE_GROUND 0.3f
#define ZLAYER_PARTICLES    0.4f
#define ZLAYER_ITEMS        0.5f
#define ZLAYER_PLANES       0.6f
#define ZLAYER_BULLETS      0.7f
#define ZLAYER_PLAYER       0.8f

#define MAPCHUNK_WIDTH      20
#define MAPCHUNK_HEIGHT     20
#define MAPCHUNK_TILESIZE   16

enum faction {
	FACTION_NEUTRAL,
	FACTION_PLAYER,
	FACTION_ENEMY,
};

enum reward_type {
	REWARDOPTION_STATS_ADD,
	REWARDOPTION_STATS_MULT,
	REWARDOPTION_HEAL,
};

enum player_attack {
	PLAYER_ATTACK_BULLETSHOT,
	PLAYER_ATTACK_BULLETSHOT_DOUBLE,
	PLAYER_ATTACK_HOMING_BURST,
	PLAYER_ATTACK_MAX,
};

typedef struct {
	char *title;
	char *description;

	enum reward_type type;
	union {
		struct {
			float shoot_cd;
			float plane_speed;
			float plane_turn_speed;
			float bullet_damage;
			float bullet_speed;
				float bullet_spread;
		} modify_stats;
	} data;
} reward_t;

// components
typedef struct {
	int *tiles;
} c_mapchunk;

typedef struct {
	vec2s p;
} c_pos;

typedef struct {
	vec2s vel;
	float speed;
	float damage;
} c_bullet;

typedef struct {
	float speed;
	float angle;

	float poof_cd_max;
	float poof_cd;
} c_plane;

typedef struct {
	ivec2 tile;
	ivec2 tile_size;
	float scale;
	float angle;

	float squish;
	float hurt;
	float z_layer;
} c_sprite;

typedef struct {
	float distance;
} c_shadow;

typedef struct {
	float shoot_cd_max;
	float shoot_cd;
	float plane_speed;
	float plane_turn_speed;
	float bullet_damage;
	float bullet_speed;
	float bullet_spread;
	enum player_attack attack;
} c_player;

typedef struct {
	enum faction faction;
} c_faction;

typedef struct {
	float rotation;
	vec2 vel;
	float scale_factor;
} c_particle;

typedef struct {
	float seconds;
} c_despawn_after;

typedef struct {
	float hp;
	float max_hp;
} c_health;

typedef struct {
	int current_frame;
	int frames;
	float secs_per_frame;
	
	// state
	float elapsed;
	int frame_offset;
} c_animation;

typedef struct {
	ecs_entity_t entity;
} c_target;

typedef struct {
	float turn_strength;
} c_homing;

//
// private functions
//

void system_move_plane(ecs_iter_t *);
void system_move_bullets(ecs_iter_t *);
void system_move_particles(ecs_iter_t *);
void system_despawn(ecs_iter_t *);
void system_unsquish(ecs_iter_t *);
void system_spawn_plane_effects(ecs_iter_t *);
void system_spawn_enemies(ecs_iter_t *);
void system_remove_mapchunks(ecs_iter_t *);
void system_animate_sprites(ecs_iter_t *);
void system_draw_sprites(ecs_iter_t *);
void system_draw_map(ecs_iter_t *);

void mapchunk_init(c_mapchunk *, int chunk_x, int chunk_y);
void mapchunk_destroy(c_mapchunk *);

void show_levelup_rewards(void);
void close_levelup_rewards(void);
void use_reward(reward_t *);
void collect_xp(float xp);

void hit_enemy(ecs_entity_t e_bullet, ecs_entity_t e_enemy, c_bullet *bullet, c_pos *pos_bullet, c_sprite *spr_enemy, c_health *hl_enemy, c_plane *pl_enemy);
void spawn_bulletshot(vec2 p, float angle, enum faction faction, float spread, float dmg, float speed);
void spawn_homing_shot(vec2 p, float angle, ecs_entity_t target, enum faction faction, float dmg, float speed);

//
// vars
//

static engine_t *g_engine; // engine reference for ecs callbacks

// assets
static int g_font;
static texture_t g_plane_tex;
static texture_t g_tiles_tex;

// rendering
static shader_t g_planes_shader;
static pipeline_t g_pipeline;

// state
static int g_mapgen_seed;
static int g_game_started;
static double g_game_timer;
static int g_picking_rewards;

static int g_level;
static float g_xp;
static const int g_xp_per_level[] = { 10, 12, 16, 24, 32, 69, 72, 84, 101, 196, 420, 666, 1024, 2250, 5000, 999999999 };
static reward_t g_levelup_rewards[3];

static float g_enemy_spawn_rate;
static float g_enemy_spawn_timer;

// entities
ecs_world_t *g_ecs;
ecs_query_t *g_enemies_query;
ecs_entity_t g_player;

// rewards
static const int g_reward_pool_length = 6;
static const reward_t g_reward_pool[] = {
	(reward_t) {
		.title = "Afterburner",
		.description = "Increase speed by 20%",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.plane_speed      = 1.2f,
			.plane_turn_speed = 1.1f,
			.shoot_cd         = 1.0f,
			.bullet_damage    = 1.0f,
			.bullet_speed     = 1.0f,
			.bullet_spread    = 1.0f,
		},
	},
	(reward_t) {
		.title = "Fast shooter",
		.description = "Fire rate up!",
		.type = REWARDOPTION_STATS_ADD,
		.data.modify_stats = {
			.shoot_cd      = -0.333f,
		},
	},
	(reward_t) {
		.title = "Minigun",
		.description = "Shoot fast, but less damage",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.shoot_cd         = 0.35f,
			.plane_speed      = 1.0f,
			.plane_turn_speed = 1.0f,
			.bullet_damage    = 0.4f,
			.bullet_speed     = 1.15f,
			.bullet_spread    = 1.0f,
		},
	},
	(reward_t) {
		.title = "Silver Bullet",
		.description = "25% more damage",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.shoot_cd         = 1.0f,
			.plane_speed      = 1.0f,
			.plane_turn_speed = 1.0f,
			.bullet_damage    = 1.25f,
			.bullet_speed     = 1.0f,
			.bullet_spread    = 1.0f,
		},
	},
	(reward_t) {
		.title = "Aerodynamics",
		.description = "Faster bullets",
		.type = REWARDOPTION_STATS_ADD,
		.data.modify_stats = {
			.bullet_speed = +1.0f,
		},
	},
	(reward_t) {
		.title = "Precision Scope",
		.description = "Accuracy +18%",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.shoot_cd         = 1.0f,
			.plane_speed      = 1.0f,
			.plane_turn_speed = 1.0f,
			.bullet_damage    = 1.25f,
			.bullet_speed     = 1.0f,
			.bullet_spread    = 0.82f,
		},
	},
};

ECS_COMPONENT_DECLARE(c_pos);
ECS_COMPONENT_DECLARE(c_bullet);
ECS_COMPONENT_DECLARE(c_plane);
ECS_COMPONENT_DECLARE(c_sprite);
ECS_COMPONENT_DECLARE(c_player);
ECS_COMPONENT_DECLARE(c_mapchunk);
ECS_COMPONENT_DECLARE(c_shadow);
ECS_COMPONENT_DECLARE(c_despawn_after);
ECS_COMPONENT_DECLARE(c_faction);
ECS_COMPONENT_DECLARE(c_particle);
ECS_COMPONENT_DECLARE(c_health);
ECS_COMPONENT_DECLARE(c_animation);
ECS_COMPONENT_DECLARE(c_target);
ECS_COMPONENT_DECLARE(c_homing);

ECS_SYSTEM_DECLARE(system_move_plane);
ECS_SYSTEM_DECLARE(system_move_bullets);
ECS_SYSTEM_DECLARE(system_move_particles);
ECS_SYSTEM_DECLARE(system_despawn);
ECS_SYSTEM_DECLARE(system_unsquish);
ECS_SYSTEM_DECLARE(system_spawn_plane_effects);
ECS_SYSTEM_DECLARE(system_spawn_enemies);
ECS_SYSTEM_DECLARE(system_remove_mapchunks);
ECS_SYSTEM_DECLARE(system_animate_sprites);
ECS_SYSTEM_DECLARE(system_draw_map);
ECS_SYSTEM_DECLARE(system_draw_sprites);

//
// scene functions
//

static void load(struct scene_planes_s *scene, struct engine_s *engine) {
	// configure engine
	engine_set_clear_color(0.87f, 0.96f, 0.96f);
	engine->console_visible = 0;
	glm_mat4_identity(engine->u_view);
	// seed rng
	g_mapgen_seed = time(0);
	srand(g_mapgen_seed);

	// rendering
	shader_init_from_dir(&g_planes_shader, "res/shader/planes/");
	pipeline_init(&g_pipeline, &g_planes_shader, 2048);

	// setup state
	g_game_started      = 0;
	g_game_timer        = 0.0;
	g_picking_rewards   = 0;
	g_level             = 0;
	g_xp                = 0.0f;
	g_enemy_spawn_rate  = 1.5f;
	g_enemy_spawn_timer = 0.0f;

	// assets
	g_font = nvgCreateFont(engine->vg, "Baloo", "res/font/Baloo-Regular.ttf");
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.flip_y = 0;
	texture_init_from_image(&g_plane_tex, "res/sprites/planes.png", &settings);

	texture_init_from_image(&g_tiles_tex, "res/sprites/plane_tiles.png", &settings);

	// ecs
	g_engine = engine;
	g_ecs = ecs_init();
	ECS_COMPONENT_DEFINE(g_ecs, c_pos);
	ECS_COMPONENT_DEFINE(g_ecs, c_bullet);
	ECS_COMPONENT_DEFINE(g_ecs, c_plane);
	ECS_COMPONENT_DEFINE(g_ecs, c_sprite);
	ECS_COMPONENT_DEFINE(g_ecs, c_player);
	ECS_COMPONENT_DEFINE(g_ecs, c_mapchunk);
	ECS_COMPONENT_DEFINE(g_ecs, c_shadow);
	ECS_COMPONENT_DEFINE(g_ecs, c_despawn_after);
	ECS_COMPONENT_DEFINE(g_ecs, c_faction);
	ECS_COMPONENT_DEFINE(g_ecs, c_particle);
	ECS_COMPONENT_DEFINE(g_ecs, c_health);
	ECS_COMPONENT_DEFINE(g_ecs, c_animation);
	ECS_COMPONENT_DEFINE(g_ecs, c_target);
	ECS_COMPONENT_DEFINE(g_ecs, c_homing);

	ECS_SYSTEM_DEFINE(g_ecs, system_move_plane,          0, c_pos, c_plane);
	ECS_SYSTEM_DEFINE(g_ecs, system_move_bullets,        0, c_pos, c_bullet, c_faction, ?c_target, ?c_homing, ?c_sprite);
	ECS_SYSTEM_DEFINE(g_ecs, system_move_particles,      0, c_pos, c_particle, c_sprite, ?c_shadow); dummy:
	ECS_SYSTEM_DEFINE(g_ecs, system_despawn,             0, c_despawn_after);
	ECS_SYSTEM_DEFINE(g_ecs, system_unsquish,            0, c_sprite);
	ECS_SYSTEM_DEFINE(g_ecs, system_spawn_plane_effects, 0, c_pos, c_plane);
	ECS_SYSTEM_DEFINE(g_ecs, system_spawn_enemies,       0, c_pos, c_player);
	ECS_SYSTEM_DEFINE(g_ecs, system_remove_mapchunks,    0, c_pos, c_mapchunk);
	ECS_SYSTEM_DEFINE(g_ecs, system_animate_sprites,     0, c_sprite, c_animation);
	ECS_SYSTEM_DEFINE(g_ecs, system_draw_sprites,        0, c_pos, c_sprite, ?c_shadow); dummy2: // fix weird query syyntax problems for my lsp
	ECS_SYSTEM_DEFINE(g_ecs, system_draw_map,            0, c_pos, c_mapchunk);

	g_enemies_query = ecs_query(g_ecs, {
		.filter.terms = {
			{ ecs_id(c_pos) }, { ecs_id(c_plane) }, { ecs_id(c_sprite) }, { ecs_id(c_faction) }, { ecs_id(c_health) }
		},
	});

	{ // spawn player
		ecs_entity_t e = ecs_new_id(g_ecs);
		g_player = e;
		ecs_set(g_ecs, e, c_pos, {
			.p.raw = { engine->window_width * 0.5f, engine->window_height * 0.5f }
		});
		ecs_set(g_ecs, e, c_plane, {
			.angle = 0.0f,
			.poof_cd_max = 0.4f,
			.poof_cd = 0.0f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {0, 1},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.0f,
			.z_layer = ZLAYER_PLAYER,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_player, {
			.shoot_cd = 0.0f,
			.shoot_cd_max = 0.75f,
			.plane_speed = 1.0f,
			.plane_turn_speed = 1.4f,
			.bullet_damage = 1.0f,
			.bullet_speed = 6.0f,
			.bullet_spread = 30.0f,
			.attack = PLAYER_ATTACK_HOMING_BURST,
		});
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_PLAYER });
	}

	for (int x = 0; x < 3; ++x) { // map
		for (int y = 0; y < 3; ++y) {
			ecs_entity_t e = ecs_new_id(g_ecs);
			ecs_set(g_ecs, e, c_pos, {
				.p.x = x * MAPCHUNK_WIDTH * MAPCHUNK_TILESIZE,
				.p.y = y * MAPCHUNK_HEIGHT * MAPCHUNK_TILESIZE,
			});
			ecs_set(g_ecs, e, c_mapchunk, { NULL });
			
			c_mapchunk *chunk = ecs_get_mut(g_ecs, e, c_mapchunk);
			mapchunk_init(chunk, x, y);
			ecs_modified(g_ecs, e, c_mapchunk);
		}
	}

	// enemies
	for (int i = 0; i < 10; ++i) {
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {100.0f, 100.0f + (float)i * 50.0f} });
		ecs_set(g_ecs, e, c_plane, {
			.speed = 1.0f,
			.angle = 0.3f,
			.poof_cd = (rand() % 700) * 0.003f,
			.poof_cd_max = 0.7f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {rand() % 4, 5},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.2f,
			.z_layer = ZLAYER_PLANES,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_ENEMY });
		ecs_set(g_ecs, e, c_health, { .hp = 2.0f, .max_hp = 2.0f });
	}

	// items
	for (int i = 0; i < 3; ++i) {
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {370.0f, 100.0f + (float)i * 50.0f} });
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {12, 1},
			.tile_size = {16, 16},
			.scale = 2.0f,
			.angle = 0.0f,
			.z_layer = ZLAYER_ITEMS,
		});
	}

	// people
	for (int i = 0; i < 10; ++i) {
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {150.0f, 150.0f + (float)i * 50.0f} });
		ecs_set(g_ecs, e, c_sprite, {
			.scale = 1.0f,
			.tile = {24, 4},
			.tile_size = {8, 8},
			.z_layer = ZLAYER_ABOVE_GROUND,
			.angle = glm_rad(-90.0f),
		});
		ecs_set(g_ecs, e, c_animation, {
			.frames = 2,
			.secs_per_frame = 0.3f,
			.frame_offset = 24,
		});
	}

}

static void destroy(struct scene_planes_s *scene, struct engine_s *engine) {
	ecs_filter_t *f = ecs_filter(g_ecs, {
		.terms = {
			{ ecs_id(c_mapchunk) },
		}
	});

	ecs_iter_t it = ecs_filter_iter(g_ecs, f);
	while (ecs_filter_next(&it)) {
		c_mapchunk *chunks = ecs_field(&it, c_mapchunk, 1);

		for (int i = 0; i < it.count; i ++) {
			mapchunk_destroy(&chunks[i]);
		}
	}

	ecs_fini(g_ecs);
	texture_destroy(&g_plane_tex);
	texture_destroy(&g_tiles_tex);

	shader_destroy(&g_planes_shader);
	pipeline_destroy(&g_pipeline);
}

static void tick(struct scene_planes_s *scene, struct engine_s *engine, float dt) {
	ecs_run(g_ecs, ecs_id(system_animate_sprites),     dt, NULL);
	ecs_run(g_ecs, ecs_id(system_move_particles),      dt, NULL);
	ecs_run(g_ecs, ecs_id(system_move_bullets),        dt, NULL);
}

static void update(struct scene_planes_s *scene, struct engine_s *engine, float dt) {
	if (!g_game_started) {
		if (engine->input_drag.state == INPUT_DRAG_BEGIN) {
			g_game_started = 1;
		}
		return;
	}

	static float gamespeed = 1.0f;
	if (g_picking_rewards) {
		gamespeed = glm_max(gamespeed - 0.025f, 0.0f);
		dt *= glm_ease_quart_in(gamespeed);
	} else {
		gamespeed = glm_min(gamespeed + 0.035f, 1.0f);
		dt *= glm_ease_quad_in(gamespeed);
	}

	g_game_timer += dt;

	// handle player controls
	if (ecs_is_alive(g_ecs, g_player)) {
		c_pos *pp = ecs_get_mut(g_ecs, g_player, c_pos);
		c_plane *pl = ecs_get_mut(g_ecs, g_player, c_plane);
		c_sprite *spr = ecs_get_mut(g_ecs, g_player, c_sprite);
		c_player *ply = ecs_get_mut(g_ecs, g_player, c_player);
		ply->shoot_cd -= dt;

		engine->u_view[3][0] = engine->window_width * 0.5f - pp->p.x;
		engine->u_view[3][1] = engine->window_height * 0.5f - pp->p.y;

		// turn player
		if (engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
			const float turn_angle = 0.035f * dt * 60.0f;

			float angle_delta = 0.0f;
			pl->speed = ply->plane_turn_speed;
			if (engine->input_drag.x < engine->window_width * 0.5f) {
				angle_delta = -1.0f;
			} else {
				angle_delta = 1.0f;
			}

			spr->angle = pl->angle += turn_angle * angle_delta;

			ecs_modified(g_ecs, g_player, c_plane);
			ecs_modified(g_ecs, g_player, c_sprite);
		} else if (engine->input_drag.state == INPUT_DRAG_END) {
			pl->speed = ply->plane_speed;
			ecs_modified(g_ecs, g_player, c_plane);
		}

		// shoot bullet
		if (ply->shoot_cd <= 0.0f) {
			ply->shoot_cd = ply->shoot_cd_max;

			spr->squish = 1.0f;
			
			switch (ply->attack) {
			case PLAYER_ATTACK_BULLETSHOT:
				spawn_bulletshot(pp->p.raw, pl->angle, FACTION_PLAYER, ply->bullet_spread, ply->bullet_damage, ply->bullet_speed);
				break;
			case PLAYER_ATTACK_BULLETSHOT_DOUBLE:{
				vec2s center = pp->p;
				vec2 forward = { cosf(pl->angle), sinf(pl->angle) };
				vec2 left, right;
				glm_vec2_rotate(forward, glm_rad(-90.0f), left);
				glm_vec2_rotate(forward, glm_rad(90.0f), right);
				const float dist = 9.5f;
				glm_vec2_scale_as(left, dist, left);
				glm_vec2_scale_as(right, dist, right);

				vec2 left_gun, right_gun;
				glm_vec2_add(center.raw, left, left_gun);
				glm_vec2_add(center.raw, right, right_gun);

				spawn_bulletshot(left_gun, pl->angle, FACTION_PLAYER, ply->bullet_spread, ply->bullet_damage, ply->bullet_speed);
				spawn_bulletshot(right_gun, pl->angle, FACTION_PLAYER, ply->bullet_spread, ply->bullet_damage, ply->bullet_speed);
				break;
			}
			case PLAYER_ATTACK_HOMING_BURST: {
				ecs_filter_t *potential_targets = ecs_filter(g_ecs, {
					.terms = { {ecs_id(c_pos)}, {ecs_id(c_plane)}, {ecs_id(c_faction)} },
					});

				ecs_entity_t targets[5] = {0};
				ecs_iter_t it = ecs_filter_iter(g_ecs, potential_targets);
				while (ecs_filter_next(&it)) {
					for (int i = 0; i < it.count; ++i) {
						if (i < 5) {
							targets[i] = it.entities[i];
						}
					}
				}

				for (int i = 0; i < 5; ++i) {
					if (targets[i] != 0 && ecs_is_alive(g_ecs, targets[i])) {
						spawn_homing_shot(pp->p.raw, glm_rad(rand() % 360), targets[i], FACTION_PLAYER, ply->bullet_damage, ply->bullet_speed);
					}
				}
			}
			case PLAYER_ATTACK_MAX: break;
			}
		}
	}

	ecs_run(g_ecs, ecs_id(system_move_plane),          dt, NULL);
	ecs_run(g_ecs, ecs_id(system_despawn),             dt, NULL);
	ecs_run(g_ecs, ecs_id(system_unsquish),            dt, NULL);
	ecs_run(g_ecs, ecs_id(system_spawn_plane_effects), dt, NULL);
	ecs_run(g_ecs, ecs_id(system_remove_mapchunks),    dt, NULL);

	g_enemy_spawn_timer += dt;
	while (g_enemy_spawn_timer >= g_enemy_spawn_rate) {
		g_enemy_spawn_timer -= g_enemy_spawn_rate;
		ecs_run(g_ecs, ecs_id(system_spawn_enemies), dt, NULL);
	}

	// tick 60x per second
	const float tick_rate = 1.0f / 60.0f;
	static float accu = 0.0f;
	while (accu >= tick_rate) {
		accu -= tick_rate;
		tick(scene, engine, tick_rate);
	}
	accu += dt;
	
}

static void draw(struct scene_planes_s *scene, struct engine_s *engine) {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ecs_run(g_ecs, ecs_id(system_draw_map), 1.0f, NULL);
	ecs_run(g_ecs, ecs_id(system_draw_sprites), 1.0f, NULL);

	const float W = engine->window_width;
	const float H = engine->window_height;
	NVGcontext *vg = engine->vg;
	if (!g_game_started) {
		// draw controls

		const float pad = 14.0f;

		// left
		nvgBeginPath(vg);
		nvgRoundedRect(vg, pad, H * 0.4f + pad, W * 0.5f - pad * 1.5f, H * 0.6f - pad * 2.0f, 15.0f);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.4f));
		nvgFill(vg);

		// right
		nvgBeginPath(vg);
		nvgRoundedRect(vg, W * 0.5f + pad * 0.5f, H * 0.4f + pad, W * 0.5f - pad * 1.5f, H * 0.6f - pad * 2.0f, 15.0f);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.4f));
		nvgFill(vg);

		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		nvgFontSize(vg, 28.0f);
		nvgFontFaceId(vg, g_font);
		// bg
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.0f));
		nvgFontBlur(vg, 1.0f);
		nvgText(vg, W * 0.25f - 2.0f, H * 0.65f + 2.0f, "Turn LEFT", NULL);
		nvgText(vg, W * 0.75f - 2.0f, H * 0.65f + 2.0f, "Turn RIGHT", NULL);
		// fg
		nvgFontBlur(vg, 0.0f);
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgText(vg, W * 0.25f, H * 0.65f, "Turn LEFT", NULL);
		nvgText(vg, W * 0.75f, H * 0.65f, "Turn RIGHT", NULL);
	} else if (g_picking_rewards) {

		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.2f));
		nvgRect(vg, 30.0f, 110.0f, W - 2*30.0f, H - 2*110.0f);
		nvgFill(vg);

		// TODO: hack.
		for (int i = 0; i < 4; ++i) {
			float bx = 30.0f + 15.0f;
			float by = 110.0f + 15.0f + (80.0f + 15.0f) * i;
			float bw = W - 2*30.0f - 2*15.0f;
			float bh = 80.0f;
			float mx = engine->input_drag.x;
			float my = engine->input_drag.y;
			if (i == 3) {
				// TODO: hack.
				by += bh * 0.5f;
			}
			int state = engine->input_drag.state;
			int is_hovering = (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh);
			
			if (state == INPUT_DRAG_END) {
				if (is_hovering) {
					// TODO: hack.
					if (i < 3) {
						use_reward(&g_levelup_rewards[i]);
					}
					close_levelup_rewards();
				}
			}

			reward_t rew;
			if (i < 3) {
				rew = g_levelup_rewards[i];
			} else {
				// TODO: hack.
				rew = (reward_t){
					.title = "~ SKIP ~",
					.description = "Don't use any reward.",
					.type = REWARDOPTION_HEAL,
				};
			}
			nvgBeginPath(vg);
			if (is_hovering) {
				nvgFillColor(vg, nvgRGBf(0.85f, 0.8f, 0.75f));
				nvgStrokeColor(vg, nvgRGBf(0.85f, 0.75f, 0.5f));
			} else {
				nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
				nvgStrokeColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
			}
			nvgStrokeWidth(vg, 4.0f);
			nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
			nvgFill(vg);
			nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
			nvgStroke(vg);

			nvgFontFaceId(vg, g_font);
			nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
			nvgFontSize(vg, 32.0f);
			nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
			nvgText(vg, bx + 10.0f, by + 50.0f, rew.title, NULL);

			nvgFontSize(vg, 16.0f);
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
			nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
			nvgText(vg, bx + 10.0f, by + 50.0f, rew.description, NULL);
		}

	} else {
		static char game_timer[16];
		int ss = (int)(g_game_timer) % 60;
		int mm = (int)(g_game_timer / 60.0);
		int ms = (int)((g_game_timer - floorf(g_game_timer)) * 1000.0);
		snprintf(game_timer, 16, "%02d:%02d.%03d", mm, ss, ms);

		// draw menu bar
		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.3f));
		nvgRoundedRect(vg, 12.0f, -40.0f, W - 2.0f * 12.0f, 40.0f + 42.0f, 4.0f);
		nvgFill(vg);

		// time
		// bg
		nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFontFaceId(vg, g_font);
		nvgFontSize(vg, 18.0f);
		nvgText(vg, 20.0f - 1.0f, 12.0f + 1.0f, game_timer, NULL);
		// fg
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFontSize(vg, 18.0f);
		nvgText(vg, 20.0f, 12.0f, game_timer, NULL);

		// draw level
		static char level[32];
		struct { float x, y, w, h; } bounds;
		snprintf(level, 32, "lv.%d (%d/%d XP)", g_level, (int)g_xp, g_xp_per_level[g_level]);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgFontSize(vg, 18.0f);
		nvgTextBounds(vg, 0.0f, 0.0f, level, NULL, (float*)&bounds);
		// bg
		nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
		nvgFontSize(vg, 18.0f);
		nvgText(vg, W - 20.0f - bounds.w - 1.0f, 12.0f + 1.0f, level, NULL);
		// fg
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgText(vg, W - 20.0f - bounds.w, 12.0f, level, NULL);

	}
}

void scene_planes_init(struct scene_planes_s *scene, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene, engine);
	
	// init function pointers
	scene->base.load    = (scene_load_fn)load;
	scene->base.destroy = (scene_destroy_fn)destroy;
	scene->base.update  = (scene_update_fn)update;
	scene->base.draw    = (scene_draw_fn)draw;
}


//
// private implementations
//

static void mapchunk_set(c_mapchunk *chunk, int x, int y, int tx, int ty) {
	chunk->tiles[x + y * MAPCHUNK_WIDTH] = tx + ty * MAPCHUNK_TILESIZE;
}
static int mapchunk_get(c_mapchunk *chunk, int x, int y, int *tx, int *ty) {
	if (x < 0 || y < 0 || x >= MAPCHUNK_WIDTH || y >= MAPCHUNK_HEIGHT) {
		return 0;
	}

	int id = chunk->tiles[x + y * MAPCHUNK_WIDTH];

	if (tx != NULL) {
		*tx = id % MAPCHUNK_TILESIZE;
	}
	if (ty != NULL) {
		*ty = (int)(id / (float)MAPCHUNK_TILESIZE);
	}

	return id;
}

void mapchunk_init(c_mapchunk *chunk, int chunk_x, int chunk_y) {
	chunk->tiles = malloc(MAPCHUNK_WIDTH * MAPCHUNK_HEIGHT * sizeof(chunk->tiles[0]));

	for (int y = 0; y < MAPCHUNK_HEIGHT; ++y) {
		for (int x = 0; x < MAPCHUNK_WIDTH; ++x) {
			int global_x = chunk_x * MAPCHUNK_WIDTH + x;
			int global_y = chunk_y * MAPCHUNK_HEIGHT + y;

			// initialize all to water
			mapchunk_set(chunk, x, y, -1, -1);

			// islands
			float v = (
					  0.8f * stb_perlin_noise3_seed((float)global_x / 12.0f, (float)global_y / 12.0f, 0.0f, 0, 0, 0, g_mapgen_seed)
					+ 0.2f * stb_perlin_noise3_seed((float)global_x /  6.0f, (float)global_y /  8.0f, 0.0f, 0, 0, 0, g_mapgen_seed)
					);

			int is_land = (v <= 0.1f);

			if (is_land) {
				mapchunk_set(chunk, x, y, 2, 4);

				// vegetation
				float r = stb_perlin_noise3_seed((float)global_x / 8.0f, (float)global_y / 8.0f, 0.5f, 0, 0, 0, 256 * v);

				if (r > 0.4f) {
					mapchunk_set(chunk, x, y, 0, 4 + rand() % 2);
				}

				// houses
				float h = stb_perlin_noise3_seed((float)global_x / 10.0f, (float)global_y / 10.0f, 0.0f, 0, 0, 0, g_mapgen_seed);
				int is_city = (h < -0.1f);

				h *= stb_perlin_noise3_seed((float)global_x / 2.5f, (float)global_y / 1.5f, 0.0f, 0, 0, 0, g_mapgen_seed);
				int is_house = (h < -0.15f);

				h = stb_perlin_noise3_seed((float)global_x / 10.0f, (float)global_y / 10.0f, 0.4f, 0, 0, 0, g_mapgen_seed)
					* stb_perlin_noise3_seed((float)global_x / 1.0f, (float)global_y / 3.0f + 0.5f, 0.4f, 0, 0, 0, g_mapgen_seed);
				int is_field = (h < -0.1f);

				if (is_land && is_city) {
					if (is_house) {
						mapchunk_set(chunk, x, y, 0, 6 + rand() % 2);
					} else if (is_field) {
						mapchunk_set(chunk, x, y, 1, 9);
					}
				}
			}

		}
	}
	
	// second pass, terrain outline
	for (int y = 0; y < MAPCHUNK_HEIGHT; ++y) {
		for (int x = 0; x < MAPCHUNK_WIDTH; ++x) {
			int tx, ty, id;
			id = mapchunk_get(chunk, x, y, &tx, &ty);
			if (id < 0) continue;

			int tiles[] = {
				// top
				mapchunk_get(chunk, x - 1, y - 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x    , y - 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x + 1, y - 1, NULL, NULL) < 0,
				// mid
				mapchunk_get(chunk, x - 1, y    , NULL, NULL) < 0,
				-1, // current tile
				mapchunk_get(chunk, x + 1, y    , NULL, NULL) < 0,
				// bottom
				mapchunk_get(chunk, x - 1, y + 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x    , y + 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x + 1, y + 1, NULL, NULL) < 0,
			};
			int top = tiles[1];
			int left = tiles[3];
			int right = tiles[5];
			int bottom = tiles[7];

			int topleft = tiles[0];
			int topright = tiles[2];
			int botleft = tiles[6];
			int botright = tiles[8];

			int ox = -left + right;
			int oy = -top + bottom;

			if (ox == 0 && oy == 0) {
				if (topleft + topright + botleft + botright > 1) {
					continue;
				}

				if (topleft) {
					ox += 3;
				} else if (topright) {
					ox += 2;
				} else if (botleft) {
					ox += 3;
					oy -= 1;
				} else if (botright) {
					ox += 2;
					oy -= 1;
				}
			}

			if (ox || oy) {
				mapchunk_set(chunk, x, y, 2 + ox, 4 + oy);
			}
		}
	}
}

void mapchunk_destroy(c_mapchunk *chunk) {
	free(chunk->tiles);
	chunk->tiles = NULL;
}

void spawn_bulletshot(vec2 p, float angle, enum faction faction, float spread, float dmg, float speed) {
	float rnd_angle = glm_rad((rand() / (float)RAND_MAX) * spread - spread * 0.5f);
	ecs_entity_t e = ecs_new_id(g_ecs);
	ecs_set(g_ecs, e, c_pos, { .p.raw = {p[0], p[1]} });
	ecs_set(g_ecs, e, c_sprite, {
			.tile = {10, 0},
			.tile_size = {16, 16},
			.angle = angle + rnd_angle,
			.scale = 1.5f,
			.z_layer = ZLAYER_BULLETS,
			});
	ecs_set(g_ecs, e, c_despawn_after, { .seconds = 5.0f });
	ecs_set(g_ecs, e, c_bullet, {
			.vel.raw = {
				cosf(angle + rnd_angle), sinf(angle + rnd_angle)
			},
			.damage = dmg,
			.speed = speed,
			});
	ecs_set(g_ecs, e, c_faction, { .faction = faction });
}

void spawn_homing_shot(vec2 p, float angle, ecs_entity_t target, enum faction faction, float dmg, float speed) {
	ecs_entity_t e = ecs_new_id(g_ecs);
	ecs_set(g_ecs, e, c_pos, { .p.raw = {p[0], p[1]} });
	ecs_set(g_ecs, e, c_sprite, {
			.tile = {11, 1},
			.tile_size = {16, 16},
			.angle = angle,
			.scale = 1.5f,
			.z_layer = ZLAYER_BULLETS,
			});
	ecs_set(g_ecs, e, c_despawn_after, { .seconds = 5.0f });
	ecs_set(g_ecs, e, c_bullet, {
			.vel.raw = {
				cosf(angle) * 2.0f, sinf(angle) * 2.0f
			},
			.damage = dmg,
			.speed = speed * 0.5f,
			});
	ecs_set(g_ecs, e, c_faction, { .faction = faction });
	ecs_set(g_ecs, e, c_target, { .entity = target });
	ecs_set(g_ecs, e, c_homing, { .turn_strength=0.1f });
}

void show_levelup_rewards(void) {
	g_picking_rewards = 1;

	for (int i = 0; i < 3; ++i) {
		g_levelup_rewards[i] = g_reward_pool[rand() % g_reward_pool_length];
	}
}

void close_levelup_rewards(void) {
	g_picking_rewards = 0;
}

void use_reward(reward_t *reward) {
	if (!ecs_is_alive(g_ecs, g_player)) {
		return;
	}

	c_plane *plane = ecs_get_mut(g_ecs, g_player, c_plane);
	c_player *player = ecs_get_mut(g_ecs, g_player, c_player);

	switch (reward->type) {
		case REWARDOPTION_STATS_ADD:
			plane->speed = (player->plane_speed += reward->data.modify_stats.plane_speed);
			player->plane_turn_speed += reward->data.modify_stats.plane_turn_speed;
			player->shoot_cd_max += reward->data.modify_stats.shoot_cd;
			player->bullet_damage += reward->data.modify_stats.bullet_damage;
			player->bullet_speed += reward->data.modify_stats.bullet_speed;
			player->bullet_spread += reward->data.modify_stats.bullet_spread;
			break;
		case REWARDOPTION_STATS_MULT:
			plane->speed = (player->plane_speed *= reward->data.modify_stats.plane_speed);
			player->plane_turn_speed *= reward->data.modify_stats.plane_turn_speed;
			player->shoot_cd_max *= reward->data.modify_stats.shoot_cd;
			player->bullet_damage *= reward->data.modify_stats.bullet_damage;
			player->bullet_speed *= reward->data.modify_stats.bullet_speed;
			player->bullet_spread *= reward->data.modify_stats.bullet_spread;
			break;
	}

	// TODO: only fire when really modified
	ecs_modified(g_ecs, g_player, c_plane);
	ecs_modified(g_ecs, g_player, c_player);
}

void collect_xp(float xp) {
	g_xp += xp;
	if (g_xp >= g_xp_per_level[g_level]) {
		g_xp -= g_xp_per_level[g_level];
		++g_level;

		show_levelup_rewards();
	}
}

void hit_enemy(ecs_entity_t e_bullet, ecs_entity_t e_enemy, c_bullet *bullet, c_pos *pos_bullet, c_sprite *spr_enemy, c_health *hl_enemy, c_plane *pl_enemy) {
	spr_enemy->hurt = 1.0f;
	hl_enemy->hp -= bullet->damage;

	// destroy bullet
	ecs_delete(g_ecs, e_bullet);

	// and create particles
	for (int i = 0; i < 5; ++i) {
		float bullet_angle = atan2f(-bullet->vel.y, -bullet->vel.x);
		float angle = bullet_angle + glm_rad( (rand() / (double)RAND_MAX) * 90.0f - 45.0f );

		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = { pos_bullet->p.x, pos_bullet->p.y } });
		ecs_set(g_ecs, e, c_despawn_after, { .seconds = 0.25f });
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {10 + (rand() & 1), 3 + (rand() & 1)},
			.tile_size = {16, 16},
			.angle = 0.0f,
			.scale = 1.5f,
			.z_layer = ZLAYER_BULLETS,
		});
		ecs_set(g_ecs, e, c_particle, {
			.rotation = 0.1f,
			.scale_factor = 0.95f,
			.vel = { cosf(angle) * 1.0f, sinf(angle) * 1.0f },
		});
	}

	// no hp left? also destroy plane
	if (hl_enemy->hp <= 0.0f) {
		// destroy plane, crash it into the ground
		ecs_set(g_ecs, e_enemy, c_particle, {
				.rotation=((rand() & 1) == 0 ? -0.1f : 0.1f),
				.vel={ cosf(pl_enemy->angle) * pl_enemy->speed, sinf(pl_enemy->angle) * pl_enemy->speed },
				.scale_factor = 0.997f,
				});
		ecs_remove(g_ecs, e_enemy, c_plane);
		c_sprite *spr = ecs_get_mut(g_ecs, e_enemy, c_sprite);
		spr->z_layer = ZLAYER_ABOVE_GROUND;
		ecs_set(g_ecs, e_enemy, c_despawn_after, { .seconds=1.0f });

		// TODO: better xp handling
		collect_xp(4.0f);
	}
}

// systems

void system_move_plane(ecs_iter_t *it) {
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_plane *planes = ecs_field(it, c_plane, 2);

	for (int i = 0; i < it->count; ++i) {
		vec2 dir = {cosf(planes[i].angle), sinf(planes[i].angle)};
		glm_vec2_scale(dir, planes[i].speed * it->delta_time * 60.0f, dir);
		glm_vec2_add(pos[i].p.raw, dir, pos[i].p.raw);
	}
}

void system_move_bullets(ecs_iter_t *it_bullet) {
	c_pos *p_bullets = ecs_field(it_bullet, c_pos, 1);
	c_bullet *bullets = ecs_field(it_bullet, c_bullet, 2);
	c_faction *fac_bullets = ecs_field(it_bullet, c_faction, 3);
	c_target *trg_bullets = NULL;
	c_homing *homing_bullets = NULL;
	c_sprite *spr_bullets = NULL;

	if (ecs_field_is_set(it_bullet, 4)) {
		trg_bullets = ecs_field(it_bullet, c_target, 4);
	}
	if (ecs_field_is_set(it_bullet, 5)) {
		homing_bullets = ecs_field(it_bullet, c_homing, 5);
	}
	if (ecs_field_is_set(it_bullet, 6)) {
		spr_bullets = ecs_field(it_bullet, c_sprite, 6);
	}


	for (int i = 0; i < it_bullet->count; ++i) {
		// get target position, if it exists
		const c_pos *target_pos = NULL;
		if (trg_bullets && ecs_is_alive(g_ecs, trg_bullets[i].entity)) {
			target_pos = ecs_get(g_ecs, trg_bullets[i].entity, c_pos);
		}
		c_homing *is_homing = (homing_bullets ? &homing_bullets[i] : NULL);

		if (is_homing && target_pos) {
			vec2s to_target;
			glm_vec2_sub(target_pos->p.raw, p_bullets[i].p.raw, to_target.raw);
			glm_vec2_normalize(to_target.raw);
			glm_vec2_scale(to_target.raw, is_homing->turn_strength * it_bullet->delta_time * 60.0f, to_target.raw);

			glm_vec2_add(bullets[i].vel.raw, to_target.raw, bullets[i].vel.raw);
			//glm_vec2(to_target.raw, bullets[i].vel.raw);

			if (spr_bullets) {
				spr_bullets[i].angle = atan2f(bullets[i].vel.y, bullets[i].vel.x);
			}
		}
		vec2s dir;
		glm_vec2_scale(bullets[i].vel.raw, bullets[i].speed * it_bullet->delta_time * 60.0f, dir.raw);
		glm_vec2_add(p_bullets[i].p.raw, dir.raw, p_bullets[i].p.raw);

		// collide with enemies
		ecs_iter_t it_enemy = ecs_query_iter(g_ecs, g_enemies_query);
		while (ecs_query_next(&it_enemy)) {
			c_pos     *p_planes      = ecs_field(&it_enemy, c_pos,     1);
			c_plane   *planes        = ecs_field(&it_enemy, c_plane,   2);
			c_sprite  *spr_planes    = ecs_field(&it_enemy, c_sprite,  3);
			c_faction *fac_planes    = ecs_field(&it_enemy, c_faction, 4);
			c_health  *health_planes = ecs_field(&it_enemy, c_health,  5);

			for (int j = 0; j < it_enemy.count; ++j) {
				if (fac_bullets[i].faction == fac_planes[j].faction) {
					continue;
				}

				vec2 to_plane;
				glm_vec2_sub(p_planes[j].p.raw, p_bullets[i].p.raw, to_plane);

				// hit!
				if (glm_vec2_norm2(to_plane) <= 25.0f * 25.0f) {
					hit_enemy(it_bullet->entities[i], it_enemy.entities[j], &bullets[i], &p_bullets[i], &spr_planes[j], &health_planes[j], &planes[j]);
				}
			}
		}
	}

}

void system_move_particles(ecs_iter_t *it) {
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_particle *particles = ecs_field(it, c_particle, 2);
	c_sprite *sprites = ecs_field(it, c_sprite, 3);
	c_shadow *shadows = NULL;

	for (int i = 0; i < it->count; ++i) {
		if (ecs_field_is_set(it, 4)) {
			shadows = ecs_field(it, c_shadow, 4);
		}

		// rotate
		sprites[i].angle += particles[i].rotation * 60.0f * it->delta_time;

		// move
		vec2 dir;
		glm_vec2_scale(particles[i].vel, 60.0f * it->delta_time, dir);
		glm_vec2_add(pos[i].p.raw, dir, pos[i].p.raw);
		
		// scale
		sprites[i].scale *= particles[i].scale_factor;

		if (shadows) {
			pos[i].p.y += 0.5f * 60.0f * it->delta_time;
			shadows[i].distance -= 0.5f * 60.0f * it->delta_time;
			shadows[i].distance = glm_max(0.0f, shadows[i].distance);
		}
	}
}
void system_despawn(ecs_iter_t *it) {
	c_despawn_after *ds = ecs_field(it, c_despawn_after, 1);

	for (int i = 0; i < it->count; ++i) {
		ds[i].seconds -= it->delta_time;
		if (ds[i].seconds <= 0.0f) {
			ecs_delete(g_ecs, it->entities[i]);
		}
	}
}

void system_unsquish(ecs_iter_t *it) {
	c_sprite *sprs = ecs_field(it, c_sprite, 1);

	for (int i = 0; i < it->count; ++i) {
		sprs[i].squish = glm_max(0.0f, sprs[i].squish - it->delta_time);
		sprs[i].hurt = glm_max(0.0f, sprs[i].hurt - it->delta_time);
	}
}

void system_spawn_plane_effects(ecs_iter_t *it) {
	c_pos *ps = ecs_field(it, c_pos, 1);
	c_plane *planes = ecs_field(it, c_plane, 2);
	for (int i = 0; i < it->count; ++i) {
		c_pos *pos = &ps[i];
		c_plane *plane = &planes[i];

		if (plane->poof_cd >= plane->poof_cd_max) {
			plane->poof_cd -= plane->poof_cd_max;

			for (int j = 0; j < 4; ++j) {
				float angle = (rand() / (float)RAND_MAX) * GLM_PI * 2.0f;
				float speed = (rand() / (float)RAND_MAX) * 0.1f;

				vec2 rnd_dir = { cosf(angle) * speed, sinf(angle) * speed };
				vec2 inv_plane_dir = { -cosf(plane->angle) * 0.04f, -sinf(plane->angle) * 0.04f };

				vec2 dir;
				glm_vec2_add(rnd_dir, inv_plane_dir, dir);

				ecs_entity_t e = ecs_new_id(g_ecs);
				ecs_set(g_ecs, e, c_pos, { .p.x = pos->p.x, .p.y = pos->p.y });
				ecs_set(g_ecs, e, c_despawn_after, { .seconds = 2.0f });
				ecs_set(g_ecs, e, c_sprite, {
					.scale = 1.0f,
					.tile = {4, rand() % 3},
					.tile_size = {32, 32},
					.z_layer = ZLAYER_PARTICLES,
				});
				ecs_set(g_ecs, e, c_particle, {
					.rotation = 0.02f,
					.vel = { dir[0], dir[1] },
					.scale_factor = 0.99f,
				});
			}
		}

		plane->poof_cd += it->delta_time;
	}
}

void system_spawn_enemies(ecs_iter_t *it) {
	//c_pos *ps = ecs_field(it, c_pos, 1);
	//c_player *players = ecs_field(it, c_player, 2);

	for (int i = 0; i < it->count; ++i) {
		//c_pos *pos = &ps[i];
		//c_player *player = &players[i];

		// spawn enemies for each player?
		
		float y = (rand() / (double)RAND_MAX) * g_engine->window_height * 0.5f;
		int plane_type = rand() % 4;

		float hp = 2.0f;
		float speed = 1.0f;
		switch (plane_type) {
		case 0:
			hp = 6.0f;
			speed = 0.9f;
			break;
		case 2:
			hp = 1.25f;
			speed = 1.1f;
			break;
		};

		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {10.0f, y} });
		ecs_set(g_ecs, e, c_plane, {
			.angle = 0.2f,
			.speed = speed,
			.poof_cd = 0.0f,
			.poof_cd_max = 0.7f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {plane_type, 5},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.2f,
			.z_layer = ZLAYER_PLANES,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_ENEMY });
		ecs_set(g_ecs, e, c_despawn_after, { .seconds=30.0f });
		ecs_set(g_ecs, e, c_health, { .hp = hp, .max_hp = hp });
	}
}

void system_remove_mapchunks(ecs_iter_t *it) {
	if (!ecs_is_alive(g_ecs, g_player)) {
		return;
	}

	c_pos *chunk_positions = ecs_field(it, c_pos, 1);
	c_mapchunk *chunks = ecs_field(it, c_mapchunk, 2);

	float wx = -g_engine->u_view[3][0];
	float wy = -g_engine->u_view[3][1];
	float ww = g_engine->window_width;
	float wh = g_engine->window_height;
	for (int i = 0; i < it->count; ++i) {
		c_pos *chunk_pos = &chunk_positions[i];
		c_mapchunk *chunk = &chunks[i];

		float x = chunk_pos->p.x;
		float y = chunk_pos->p.y;
		float w = MAPCHUNK_WIDTH * MAPCHUNK_TILESIZE;
		float h = MAPCHUNK_HEIGHT * MAPCHUNK_TILESIZE;

		if (x + w < wx || x > wx + ww || y + h < wy || y > wy + wh) {
			ecs_delete(g_ecs, it->entities[i]);
		}
	}
}

void system_animate_sprites(ecs_iter_t *it) {
	c_sprite *sprites = ecs_field(it, c_sprite, 1);
	c_animation *animations = ecs_field(it, c_animation, 2);

	for (int i = 0; i < it->count; ++i) {
		c_sprite *spr = &sprites[i];
		c_animation *anim = &animations[i];

		spr->tile[0] = anim->frame_offset + anim->current_frame;
		while (anim->elapsed >= anim->secs_per_frame) {
			anim->current_frame = ++anim->current_frame % anim->frames;
			anim->elapsed -= anim->secs_per_frame;
		}

		anim->elapsed += it->delta_time;
	}
}

void system_draw_sprites(ecs_iter_t *it) {
	pipeline_reset(&g_pipeline);
	g_pipeline.texture = &g_plane_tex;

	glDisable(GL_DEPTH_TEST);
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_sprite *sprite = ecs_field(it, c_sprite, 2);
	c_shadow *shadow = NULL;

	for (int i = 0; i < it->count; ++i) {
		if (ecs_field_is_set(it, 3)) {
			shadow = ecs_field(it, c_shadow, 3);
		}

		const float w = sprite[i].tile_size[0] * sprite[i].scale;
		const float h = sprite[i].tile_size[1] * sprite[i].scale;

		// squish
		float sq_amount = 0.2f;
		float sq_amount_inv = (1.0f - sq_amount);
		float sqw = w * sq_amount_inv + w * sq_amount * glm_ease_elast_out(1.0f - sprite[i].squish);
		float sqh = h * sq_amount_inv + h * sq_amount * glm_ease_elast_out(1.0f - sprite[i].squish);

		// hurt
		float hurt_color = glm_ease_exp_in(sprite[i].hurt);

		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = sqw;
		cmd.size.y = sqh;
		cmd.angle = sprite[i].angle + GLM_PI * 0.5f;
		drawcmd_set_texture_subrect_tile(&cmd, g_pipeline.texture, sprite[i].tile_size[0], sprite[i].tile_size[1], sprite[i].tile[0], sprite[i].tile[1]);

		// draw shadow
		if (shadow != NULL) {
			float ox = -shadow[i].distance * 0.25f;
			float oy = shadow[i].distance;

			cmd.position.x = pos[i].p.x - sqw * 0.5f + ox;
			cmd.position.y = pos[i].p.y - sqh * 0.5f + oy;
			cmd.position.z = ZLAYER_SHADOW;
			glm_vec3_zero(cmd.color_mult);
			cmd.color_mult[3] = 0.25f;
			pipeline_emit(&g_pipeline, &cmd);
		}

		// draw plane
		cmd.position.x = pos[i].p.x - sqw * 0.5f;
		cmd.position.y = pos[i].p.y - sqh * 0.5f;
		cmd.position.z = sprite[i].z_layer;
		glm_vec4_one(cmd.color_mult);
		glm_vec3_fill(cmd.color_add, hurt_color);
		pipeline_emit(&g_pipeline, &cmd);
	}

	// test drawing planes
	glEnable(GL_DEPTH_TEST);
	pipeline_draw(&g_pipeline, g_engine);
}

void system_draw_map(ecs_iter_t *it) {
	c_pos *ps = ecs_field(it, c_pos, 1);
	c_mapchunk *cs = ecs_field(it, c_mapchunk, 2);
	for (int i = 0; i < it->count; ++i) {
		pipeline_reset(&g_pipeline);
		g_pipeline.texture = &g_tiles_tex;

		// TODO: store pipeline in c_mapchunk
		drawcmd_t cmd = DRAWCMD_INIT;
		for (int y = 0; y < MAPCHUNK_HEIGHT; ++y) {
			for (int x = 0; x < MAPCHUNK_WIDTH; ++x) {
				int id = cs[i].tiles[x + y * MAPCHUNK_WIDTH];
				if (id < 0) {
					continue;
				}

				int tx = id % MAPCHUNK_TILESIZE;
				int ty = (int)(id / MAPCHUNK_TILESIZE);

				cmd.position.x = ps[i].p.x + (x * MAPCHUNK_TILESIZE);
				cmd.position.y = ps[i].p.y + (y * MAPCHUNK_TILESIZE);
				cmd.size.x = cmd.size.y = MAPCHUNK_TILESIZE;
				drawcmd_set_texture_subrect_tile(&cmd, g_pipeline.texture, MAPCHUNK_TILESIZE, MAPCHUNK_TILESIZE, tx, ty);
				pipeline_emit(&g_pipeline, &cmd);
			}
		}

		pipeline_draw(&g_pipeline, g_engine);
	}
}

