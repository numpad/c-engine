#include "experiments.h"

#include <stdlib.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <cglm/cglm.h>
#include <box2d/box2d.h>
#include <stb_ds.h>
#include "engine.h"

//
// structs & enums
//

enum tile {
	TILE_CHEESE = 0,
	TILE_CUPCAKE,   TILE_ONION,     TILE_BOBA,         TILE_TACO,           TILE_CORN,
	TILE_ORANGE,    TILE_LEMON,     TILE_BANANA,       TILE_APPLE,          TILE_APPLE_GREEN,
	TILE_FRIES,     TILE_COOKIE,    TILE_CHOCOLATE,    TILE_BONBON,         TILE_LOLLIPOP,
	TILE_PUDDING,   TILE_KIWI,      TILE_MANGO,        TILE_CHILI,          TILE_MUSHROOM,
	TILE_TOMATO,    TILE_AUBERGINE, TILE_GRAPES,       TILE_DEWMELON_SLICE, TILE_MELON_SLICE,
	TILE_PEACH,     TILE_CHERRIES,  TILE_STRAWBERRY,   TILE_BURGER,         TILE_PIZZA,
	TILE_DANGO,     TILE_NIGIRI,    TILE_FRIED_SHRIMP, TILE_SOFTICE,        TILE_DONUT,
	TILE_CROISSANT, TILE_POTATO,    TILE_CARROT,       TILE_KEBAP,          TILE_PRETZEL,
	TILE_MEAT,      TILE_PAPRIKA,   TILE_OLIVE,
};

struct emoji {
	vec2 pos;
	float scale;
	enum tile tile;
};

//
// private functions
//

// rendering
static void draw_box(NVGcontext *, float x, float y, float w, float h);
static void draw_box_title(NVGcontext *, float x, float y, float w, float h, const char *title);
static void draw_score(NVGcontext *, float x, float y, int score);
static void draw_game_container(NVGcontext *);
static void draw_tile(NVGcontext *vg, enum tile, float x, float y, float scale, float angle);
static void draw_emoji_cheatsheet(NVGcontext *vg, float x, float y, float w);
static void draw_input(NVGcontext *vg);

// emoji
static void emoji_spawn(struct emoji *);
static struct emoji *emoji_new(float x, float y);
static void emoji_delete(struct emoji *emoji);

//
// vars
//

static const NVGcolor palette[] = {
	(NVGcolor){ .r =   0  / 255.0f, .g =  48 / 255.0f, .b =  73 / 255.0f, .a = 1.0f },
	(NVGcolor){ .r = 214  / 255.0f, .g =  40 / 255.0f, .b =  40 / 255.0f, .a = 1.0f },
	(NVGcolor){ .r = 247  / 255.0f, .g = 127 / 255.0f, .b =   0 / 255.0f, .a = 1.0f },
	(NVGcolor){ .r = 252  / 255.0f, .g = 191 / 255.0f, .b =  73 / 255.0f, .a = 1.0f },
	(NVGcolor){ .r = 234  / 255.0f, .g = 226 / 255.0f, .b = 183 / 255.0f, .a = 1.0f },
};

static const int TILESET_COLUMNS = 8;
static const int TILESET_TILESIZE = 256;
static const float UI_ROW_HEIGHT = 50.0f;
static const float BOARD_WIDTH = 185.0f * 2.0f;
static const float BOARD_HEIGHT = 400.0f;
static const int tiles[] = {TILE_APPLE_GREEN, TILE_MANGO, TILE_ORANGE, TILE_MELON_SLICE, TILE_STRAWBERRY, TILE_GRAPES, TILE_PEACH, -1};

static int font_score;
static int img_food;

// game state
static int game_score;
static float game_score_anim = 0.0f;
static struct emoji **emojis;
static size_t emojis_len;

static struct emoji *dragged_emoji;
static float dragged_emoji_anim = 0.0f;
static int dragging;
static float dragging_x;

static vec2 camera_pos;

// physics
static b2WorldDef world_def;
static b2WorldId world_id;
// ground
static b2BodyDef ground_bodydef;
static b2BodyId ground_bodyid;
static b2Polygon ground_polygon;
static b2ShapeDef ground_shapedef;
// left wall
static b2BodyDef leftwall_bodydef;
static b2BodyId leftwall_bodyid;
static b2Polygon leftwall_polygon;
static b2ShapeDef leftwall_shapedef;
// right wall
static b2BodyDef rightwall_bodydef;
static b2BodyId rightwall_bodyid;
static b2Polygon rightwall_polygon;
static b2ShapeDef rightwall_shapedef;

static b2BodyDef  *body_defs;
static b2BodyId   *body_ids;
static b2ShapeDef *body_shape_defs;

//
// scene functions
//

static void load(struct scene_experiments_s *scene, struct engine_s *engine) {
	// configure engine
	engine_set_clear_color(palette[3].r, palette[3].g, palette[3].b);
	engine->console_visible = 0;
	
	// assets
	font_score = nvgCreateFont(engine->vg, "PlaypenSans", "res/font/PlaypenSans-Medium.ttf");
	img_food = nvgCreateImage(engine->vg, "res/sprites/food-emojis.png", NVG_IMAGE_GENERATE_MIPMAPS | NVG_IMAGE_FLIPY);

	// init
	game_score      = 0;
	game_score_anim = 0.0f;
	emojis          = NULL;
	emojis_len      = 0;
	dragged_emoji   = NULL;
	dragged_emoji_anim = 0.0f;
	dragging        = 0;
	dragging_x      = 0.0f;
	body_defs       = NULL;
	body_ids        = NULL;
	body_shape_defs = NULL;

	// init physics
	world_def = b2DefaultWorldDef();
	world_def.gravity = (b2Vec2){ 0.0f, 10.0f };
	world_id = b2CreateWorld(&world_def);

	{ // ground
		ground_bodydef = b2_defaultBodyDef;
		ground_bodydef.type = b2_staticBody;
		ground_bodydef.position = (b2Vec2){0.0f, 267.5f};
		ground_bodyid = b2CreateBody(world_id, &ground_bodydef);

		ground_polygon = b2MakeBox(200.0f, 20.0f);

		ground_shapedef = b2_defaultShapeDef;
		ground_shapedef.friction = 0.7f;
		ground_shapedef.restitution = 0.5f;
		b2CreatePolygonShape(ground_bodyid, &ground_shapedef, &ground_polygon);
	}
	{ // left
		leftwall_bodydef = b2_defaultBodyDef;
		leftwall_bodydef.type = b2_staticBody;
		leftwall_bodydef.position = (b2Vec2){-200.0f, 100.0f};
		leftwall_bodyid = b2CreateBody(world_id, &leftwall_bodydef);

		leftwall_polygon = b2MakeBox(20.0f, 350.0f);

		leftwall_shapedef = b2_defaultShapeDef;
		leftwall_shapedef.friction = 0.7f;
		leftwall_shapedef.restitution = 0.5f;
		b2CreatePolygonShape(leftwall_bodyid, &leftwall_shapedef, &leftwall_polygon);
	}
	{ // right
		rightwall_bodydef = b2_defaultBodyDef;
		rightwall_bodydef.type = b2_staticBody;
		rightwall_bodydef.position = (b2Vec2){200.0f, 100.0f};
		rightwall_bodyid = b2CreateBody(world_id, &rightwall_bodydef);

		rightwall_polygon = b2MakeBox(20.0f, 350.0f);

		rightwall_shapedef = b2_defaultShapeDef;
		rightwall_shapedef.friction = 0.7f;
		rightwall_shapedef.restitution = 0.5f;
		b2CreatePolygonShape(rightwall_bodyid, &rightwall_shapedef, &rightwall_polygon);
	}


	// setup camera
	glm_vec2_zero(camera_pos);
	dragged_emoji = emoji_new(0.0f, 0.0f);
	dragged_emoji->tile = TILE_APPLE;
}

static void destroy(struct scene_experiments_s *scene, struct engine_s *engine) {
	nvgDeleteImage(engine->vg, img_food);

	// free physics world
	stbds_arrfree(body_defs);
	stbds_arrfree(body_ids);
	stbds_arrfree(body_shape_defs);
	b2DestroyWorld(world_id);

	// free emojis
	for (size_t i = 0; i < emojis_len; ++i) {
		emoji_delete(emojis[i]);
	}
	stbds_arrfree(emojis);

}

static void update(struct scene_experiments_s *scene, struct engine_s *engine, float dt) {
	if (engine->input_drag.state == INPUT_DRAG_BEGIN) {
		dragging = 1;
		dragging_x = engine->input_drag.begin_x;
	} else if (engine->input_drag.state == INPUT_DRAG_END) {
		dragging = 0;

		// drop emoji
		if (dragged_emoji != NULL) {
			// setup emoji
			dragged_emoji->pos[0] = dragging_x - camera_pos[0];
			dragged_emoji->pos[1] = -250.0f;
			dragged_emoji->scale = 0.35f;

			emoji_spawn(dragged_emoji);
			dragged_emoji = NULL;

			game_score += 10;
			game_score_anim = 0.0f;

			// set next emoji
			dragged_emoji = emoji_new(0.0f, 0.0f);
			dragged_emoji_anim = 0.0f;
			// select weighted random
			dragged_emoji->tile = 0;
			for (size_t i = 0; tiles[i] >= 0; ++i) {
				if ((rand() & 1) == 0) {
					dragged_emoji->tile = tiles[i];
					break;
				}
			}
			// TODO: dragged_emoji coult be unset
		}
	}

	if (dragging && engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
		dragging_x = engine->input_drag.x;
	}

	// test physics
	for (int i = 0; i < 2; ++i) {
		b2World_Step(world_id, 1.0f / 20.0f, 8, 3);
	}

	// camera
	camera_pos[0] = engine->window_width * 0.5f;
	camera_pos[1] = engine->window_height * 0.5f;
}

static void draw(struct scene_experiments_s *scene, struct engine_s *engine) {
	const float W = engine->window_width;
	const float H = engine->window_height;
	NVGcontext *vg = engine->vg;

	// score
	draw_score(vg, W * 0.5f, 40.0f, game_score);
	// upcoming emoji
	draw_box_title(vg, W - 56.0f, 40.0f, 76.0f, UI_ROW_HEIGHT, "next");
	if (dragged_emoji != NULL) {
		dragged_emoji_anim = glm_min(dragged_emoji_anim + 0.01f, 1.0f);
		draw_tile(vg, dragged_emoji->tile, W - 56.0f, 64.0f, 0.09f + 0.085f * glm_ease_elast_out(dragged_emoji_anim), 0.0f);
	}
	// cheatsheet
	draw_emoji_cheatsheet(vg, W * 0.5f, H - 80.0f, 350.0f);

	// draw game
	draw_game_container(vg);

	// draw emojis
	nvgSave(vg);
	nvgTranslate(vg, camera_pos[0], camera_pos[1]);

	// physics objects
	const size_t bodies_len = stbds_arrlen(body_ids);
	for (size_t i = 0; i < bodies_len; ++i) {
		b2Vec2 p = b2Body_GetPosition(body_ids[i]);
		float angle = b2Body_GetAngle(body_ids[i]);

		draw_tile(vg, emojis[i]->tile, p.x, p.y, emojis[i]->scale, angle);
	}

	nvgRestore(vg);

	// draw input
	draw_input(vg);
}

void scene_experiments_init(struct scene_experiments_s *scene, struct engine_s *engine) {
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

static void draw_box(NVGcontext *vg, float x, float y, float w, float h) {
	const NVGcolor *primary = &palette[0];
	const NVGcolor *background = &palette[4];
	const NVGcolor *shadow = &palette[2];

	// draw score
	nvgSave(vg);

	// box shadow
	nvgFillColor(vg, *shadow);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x - (w * 0.5f + 2.5f), y + 5.0f, w + 5.0f, h, 10.0f);
	nvgFill(vg);

	// box
	nvgFillColor(vg, *background);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x - (w * 0.5f), y, w, h, 8.0f);
	nvgFill(vg);
	nvgRestore(vg);

	// box outline
	nvgStrokeColor(vg, *primary);
	nvgStrokeWidth(vg, 2.0f);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x - (w * 0.5f), y, w, h, 8.0f);
	nvgStroke(vg);
	nvgRestore(vg);

	nvgRestore(vg);
}

static void draw_box_title(NVGcontext *vg, float x, float y, float w, float h, const char *title) {
	draw_box(vg, x, y, w, h);

	nvgSave(vg);

	// configure font
	nvgFontSize(vg, 17.0f);
	nvgFontFaceId(vg, font_score);
	nvgTextAlign(vg, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
	// calculate text bounds
	float bounds[4];
	nvgTextBounds(vg, 0.0f, 0.0f, title, NULL, bounds);

	const float title_w = bounds[2] - bounds[0];
	const float title_x = x - (w * 0.5f) + 16.0f;
	const float title_padding = 5.0f;
	
	// text bg (outer)
	nvgBeginPath(vg);
	nvgFillColor(vg, palette[3]);
	nvgRect(vg, title_x - title_padding, y - 5.0f, title_w + title_padding * 2.0f, 6.0f);
	nvgFill(vg);
	// text bg (inner)
	nvgBeginPath(vg);
	nvgFillColor(vg, palette[4]);
	nvgRect(vg, title_x - title_padding, y, title_w + title_padding * 2.0f, 6.0f);
	nvgFill(vg);

	// text
	nvgFillColor(vg, palette[0]);
	nvgText(vg, title_x, y + 1.0f, title, NULL);

	nvgRestore(vg);
}

static void draw_score(NVGcontext *vg, float x, float y, int score) {
	// score to string
	int score_len = snprintf(NULL, 0, "%d", score);
	char score_str[score_len + 1];
	snprintf(score_str, score_len + 1, "%d", score);

	const NVGcolor *primary = &palette[0];
	const float box_w = 175.0f;

	game_score_anim = glm_min(game_score_anim + 0.017f, 1.0f);

	// background
	draw_box(vg, x, y, box_w, UI_ROW_HEIGHT);

	// text
	nvgSave(vg);

	nvgFontFaceId(vg, font_score);
	nvgFillColor(vg, *primary);
	nvgFontSize(vg, 32.0f * glm_ease_elast_out(game_score_anim));
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgText(vg, x, y + 28.0f, score_str, NULL);

	nvgRestore(vg);
}

static void draw_game_container(NVGcontext *vg) {
	const float y = -150.0f;
	const float con_w2 = BOARD_WIDTH * 0.5f;
	const float con_h = BOARD_HEIGHT;

	const float shadow_x = 6.0f;
	const float shadow_y = 6.0f;

	nvgSave(vg);
	nvgTranslate(vg, camera_pos[0], camera_pos[1]);

	// shadow border
	NVGcolor c = palette[2];
	c.a = 0.4f;
	nvgStrokeColor(vg, c);
	nvgLineCap(vg, NVG_ROUND);
	nvgStrokeWidth(vg, 6.0f);

	// draw sides right, bottom.
	nvgBeginPath(vg);
	nvgMoveTo(vg, + con_w2 + shadow_x, y +         shadow_y);
	nvgLineTo(vg, + con_w2 + shadow_x, y + con_h + shadow_y);
	nvgMoveTo(vg, - con_w2 + shadow_x, y + con_h + shadow_y);
	nvgLineTo(vg, + con_w2 + shadow_x, y + con_h + shadow_y);
	nvgStroke(vg);

	// fill bg
	nvgBeginPath(vg);
	nvgFillColor(vg, palette[4]);
	nvgRect(vg, -con_w2, y, con_w2 * 2, con_h);
	nvgFill(vg);

	// particles
	static float t = 0.0f;
	t += 1.0f / 30.0f;
	for (size_t i = 0; i < 15; ++i) {
		if (i == 0 || i == 14) continue;
		const float p = (float)i / 14;

		nvgBeginPath(vg);
		nvgCircle(vg, -con_w2 + p * con_w2 * 2.0f, y + sinf(t + p * 5.0f) * -4.0f, 20.0f + sinf(t + p * 3.0f) * 5.0f);
		nvgFill(vg);
	}

	// main border
	nvgStrokeColor(vg, palette[0]);
	nvgLineCap(vg, NVG_ROUND);
	nvgStrokeWidth(vg, 7.0f);

	// draw sides left, right, bottom
	nvgBeginPath(vg);
	nvgMoveTo(vg, -con_w2, y + 0.0f);
	nvgLineTo(vg, -con_w2, y + con_h);
	nvgMoveTo(vg, +con_w2, y + 0.0f);
	nvgLineTo(vg, +con_w2, y + con_h);
	nvgMoveTo(vg, -con_w2, y + con_h);
	nvgLineTo(vg, +con_w2, y + con_h);
	nvgStroke(vg);

	nvgRestore(vg);
}

static void draw_tile(NVGcontext *vg, enum tile tile, float x, float y, float scale, float angle) {
	const float tilesize = TILESET_TILESIZE * scale;

	nvgSave(vg);
	nvgTranslate(vg, x - tilesize * 0.5f, y - tilesize * 0.5f);

	nvgTranslate(vg, tilesize * 0.5f, tilesize * 0.5f);
	nvgRotate(vg, angle);
	nvgTranslate(vg, -tilesize * 0.5f, -tilesize * 0.5f);

	const float tx = tile % TILESET_COLUMNS;
	const float ty = floorl(tile / (float)TILESET_COLUMNS);
	const NVGpaint paint = nvgImagePattern(vg,
			-tx * tilesize, -ty * tilesize,
			tilesize * TILESET_COLUMNS, tilesize * TILESET_COLUMNS,
			0.0f, img_food, 1.0f);

	// image
	nvgBeginPath(vg);
	nvgFillPaint(vg, paint);
	nvgRect(vg, 0.0f, 0.0f, tilesize, tilesize);
	nvgFill(vg);

	nvgRestore(vg);
}

static void draw_emoji_cheatsheet(NVGcontext *vg, float x, float y, float w) {
	draw_box_title(vg, x, y, w, UI_ROW_HEIGHT, "emoji order");

	for (size_t i = 0; tiles[i] >= 0; ++i) {
		const float px = x - w * 0.5f + i * 48.0f + 32.0f;
		const float py = y + 25.0f;
		const float off_x =  (tiles[i] == TILE_MELON_SLICE ? 3.0f : 0.0f);
		const float off_y = -(tiles[i] == TILE_MELON_SLICE ? 2.0f : 0.0f);
		
		draw_tile(vg, tiles[i], px + off_x, py + off_y, 0.15f, 0.0f);
		if (tiles[i + 1] >= 0) {
			nvgBeginPath(vg);
			nvgStrokeWidth(vg, 2.0f);
			nvgStrokeColor(vg, palette[0]);
			nvgLineCap(vg, NVG_ROUND);
			nvgMoveTo(vg, px + 24.0f - 4.0f, py - 6.0f);
			nvgLineTo(vg, px + 24.0f + 3.0f, py);
			nvgMoveTo(vg, px + 24.0f - 4.0f, py + 6.0f);
			nvgLineTo(vg, px + 24.0f + 3.0f, py);
			nvgStroke(vg);
		}
	}
}

static void draw_input(NVGcontext *vg) {
	const NVGcolor *primary = &palette[0];
	const NVGcolor *background = &palette[4];
	const NVGcolor *shadow = &palette[2];
	const float preview_r = 36.0f;
	
	if (dragging) {
		nvgSave(vg);
		nvgTranslate(vg, dragging_x, camera_pos[1] - 250.0f);
		nvgStrokeWidth(vg, 2.0f);

		// preview shadow
		nvgFillColor(vg, *shadow);
		nvgBeginPath(vg);
		nvgCircle(vg, 0.0f, 0.0f + 5.0f, preview_r + 1.0f);
		nvgFill(vg);
		nvgRestore(vg);
	}


	if (dragging) {
		nvgSave(vg);
		nvgTranslate(vg, dragging_x, camera_pos[1] - 250.0f);

		// preview bg
		nvgFillColor(vg, *background);
		nvgBeginPath(vg);
		nvgCircle(vg, 0.0f, 0.0f, preview_r);
		nvgFill(vg);

		// maybe draw emoji
		if (dragged_emoji != NULL) {
			static float t = 0.0f;
			t += 0.05f;
			const float s = glm_ease_quad_inout(sinf(t));
			const float cs = cosf(t);
			draw_tile(vg, dragged_emoji->tile, 0.0f, -1.0f, 0.2f + (s * 0.02f), glm_rad(0.0f + cs * 16.0f));
		}

		// preview outline
		nvgStrokeColor(vg, *primary);
		nvgBeginPath(vg);
		nvgCircle(vg, 0.0f, 0.0f, preview_r);
		nvgStroke(vg);

		// line down
		nvgStrokeColor(vg, palette[0]);

		nvgBeginPath(vg);
		nvgMoveTo(vg, 0.0f, preview_r);
		nvgLineTo(vg, 0.0f, 499.0f);
		nvgStroke(vg);

		nvgRestore(vg);
	}
}

static void emoji_spawn(struct emoji *e) {
	// add emoji (old)
	stbds_arrput(emojis, e);
	emojis_len = stbds_arrlen(emojis);

	// add physics body
	b2BodyDef body_def = b2_defaultBodyDef;
	body_def.type = b2_dynamicBody;
	body_def.position = (b2Vec2){e->pos[0], e->pos[1]};
	body_def.linearVelocity = (b2Vec2){0.0f, 20.0f};
	b2BodyId body_id = b2CreateBody(world_id, &body_def);
	
	b2Circle body_circle;
	body_circle.radius = 75.0f * e->scale;
	body_circle.point = (b2Vec2){0.0f, 0.0f};

	b2ShapeDef body_shape_def = b2_defaultShapeDef;
	body_shape_def.density = 0.5f;
	body_shape_def.friction = 0.7f;
	b2CreateCircleShape(body_id, &body_shape_def, &body_circle);

	// add to world
	body_def.userData = (void *)stbds_arrlen(body_defs);
	stbds_arrpush(body_defs, body_def);
	stbds_arrpush(body_ids, body_id);
	stbds_arrpush(body_shape_defs, body_shape_def);
}

static struct emoji *emoji_new(float x, float y) {
	struct emoji *e = malloc(sizeof(*e));

	e->pos[0] = x;
	e->pos[1] = y;
	e->tile = TILE_MELON_SLICE;
	e->scale = 0.5f;

	return e;
}

static void emoji_delete(struct emoji *emoji) {
	// find our emoji
	for (size_t i = 0; i < emojis_len; ++i) {
		struct emoji *other = emojis[i];
		if (emoji == other) {
			stbds_arrdel(emojis, i);
			emojis_len = stbds_arrlen(emojis);
			break;
		}
	}
}

