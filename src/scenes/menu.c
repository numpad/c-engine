#include "menu.h"
#include <math.h>
#include <SDL.h>
#include <nanovg.h>
#include <cglm/cglm.h>
#include "engine.h"
#include "scenes/scene_battle.h"
#include "util/easing.h"
#include "game/isoterrain.h"

float easeOutExpo(float x) {
	return x == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
}

static void switch_to_game_scene(struct engine_s *engine) {
	struct scene_battle_s *game_scene_battle = malloc(sizeof(struct scene_battle_s));
	scene_battle_init(game_scene_battle, engine);
	engine_setscene(engine, (struct scene_s *)game_scene_battle);
}

static void gui_titlebutton(NVGcontext *vg, struct engine_s *engine, float x, float y, const char *text, int enabled) {
	const float width = 200.0f;
	const float height = 70.0f;
	const float edge_width = 25.0f;

	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y);
	nvgLineTo(vg, x, y + height);
	nvgLineTo(vg, x - width, y + height);
	nvgLineTo(vg, x - width - edge_width, y + height * 0.5f);
	nvgLineTo(vg, x - width, y);
	nvgFillColor(vg, nvgRGBAf(0.12f, 0.12f, 0.12f, 0.85f));
	nvgFill(vg);

	// text shadow
	if (enabled) {
		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBAf(0.5f, 0.5f, 0.7f, 0.7f));
		nvgFontBlur(vg, 5.0f);
		nvgFontSize(vg, 32.0f);
		nvgText(vg, x - width + 15.0f, y + height * 0.67f, text, NULL);
	}

	// text
	nvgBeginPath(vg);
	if (enabled) {
		nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 1.0f));
	} else {
		nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.4f));
	}
	nvgFontBlur(vg, 0.0f);
	nvgFontSize(vg, 32.0f);
	nvgText(vg, x - width + 15.0f, y + height * 0.67f, text, NULL);

}

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
	menu->vg_font = nvgCreateFont(engine->vg, "PermanentMarker Regular", "res/font/PermanentMarker-Regular.ttf");
	menu->vg_gamelogo = nvgCreateImage(engine->vg, "res/image/logo_placeholder.png", 0);

	menu->terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init_from_file(menu->terrain, "res/data/levels/map2.json");
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
	isoterrain_destroy(menu->terrain);
	nvgDeleteImage(engine->vg, menu->vg_gamelogo);
	free(menu->terrain);
}

static struct input_drag_s prev_drag;
static int has_prev_drag = 0;
static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
	if (engine->input_drag.state == INPUT_DRAG_BEGIN) {
		has_prev_drag = 0;
	}
	if (engine->input_drag.state == INPUT_DRAG_END) {
		has_prev_drag = 1;
		prev_drag = engine->input_drag;
	}

	if (engine->input_drag.state == INPUT_DRAG_END) {
		
	}
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	struct NVGcontext *vg = engine->vg;

	int mx, my;
	Uint32 mousebtn = SDL_GetMouseState(&mx, &my);

	struct input_drag_s *drag = &engine->input_drag;
	if (has_prev_drag) {
		drag = &prev_drag;
	}

	if (drag->state != INPUT_DRAG_NONE) {
		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBf(0.0f, 1.0f, 0.0f));
		nvgCircle(vg, drag->begin_x, drag->begin_y, 20.0f);
		nvgFill(vg);

		if (drag->state == INPUT_DRAG_IN_PROGRESS) {
			nvgBeginPath(vg);
			nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.0f));
			nvgCircle(vg, drag->x, drag->y, 10.0f);
			nvgFill(vg);
		}

		if (drag->state == INPUT_DRAG_END) {
			nvgBeginPath(vg);
			nvgFillColor(vg, nvgRGBf(1.0f, 0.0f, 0.0f));
			nvgCircle(vg, drag->end_x, drag->end_y, 15.0f);
			nvgFill(vg);
		}
	}

	// draw terrain
	glm_mat4_identity(engine->u_view);
	glm_scale(engine->u_view, (vec3){ 0.5f, 0.5f, 0.5f });
	glm_translate(engine->u_view, (vec3){ 0.0f, engine->window_aspect * -2.333f, 0.0f });
	isoterrain_draw(menu->terrain, engine->u_projection, engine->u_view);

	// draw logo
	const float xcenter = engine->window_width * 0.5f;
	const float ystart = 50.0f;
	const float width_title = engine->window_width * 0.8f;
	const float height_title = width_title * 0.5f;
	nvgBeginPath(vg);
	nvgRect(vg, xcenter - width_title * 0.5f, ystart, width_title, height_title);
	NVGpaint paint = nvgImagePattern(vg, xcenter - width_title * 0.5f, ystart, width_title, height_title, 0.0f, menu->vg_gamelogo, 1.0f);
	nvgFillPaint(vg, paint);
	nvgFill(vg);

	// continue button
	gui_titlebutton(vg, engine, engine->window_width, engine->window_height * 0.5f + 100.0f, "Continue", 0);
	gui_titlebutton(vg, engine, engine->window_width, engine->window_height * 0.5f + 180.0f, "New game", 1);
	gui_titlebutton(vg, engine, engine->window_width, engine->window_height * 0.5f + 260.0f, "Settings", 1);
	

	// play button
	static float playpress_lin = 0.0f;
	const float playpress = ease_back_out(playpress_lin);
	const float playpress2 = easeOutExpo(playpress_lin);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f + 8.0f * playpress2, engine->window_height - 500.0f + 12.0f * playpress, engine->window_width - 100.0f - 16.0f * playpress2, 75.0f - 24.0f * playpress, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 500.0f, 0.0f, engine->window_height - 425.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);
	if (mousebtn & SDL_BUTTON(1)) {
		if (mx > 50.0f && mx < engine->window_width - 50.0f && my > engine->window_height - 500.0f && my < engine->window_height - 425.0f) {
			playpress_lin += 0.3f;
			if (playpress_lin > 1.0f) {
				playpress_lin = 1.0f;

				switch_to_game_scene(engine);
			}
		}
	} else {
		playpress_lin *= 0.85f;
	}

	// play game
	float bounds[4];
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	nvgFontSize(vg, 32.0f - 4.0f * playpress);
	nvgFontBlur(vg, 0.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Play", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 450.0f, "Play", NULL);

}

void menu_init(struct menu_s *menu, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)menu, engine);

	// init function pointers
	menu->base.load = (scene_load_fn)menu_load;
	menu->base.destroy = (scene_destroy_fn)menu_destroy;
	menu->base.update = (scene_update_fn)menu_update;
	menu->base.draw = (scene_draw_fn)menu_draw;

	// init gui
	menu->gui = NULL;
}

