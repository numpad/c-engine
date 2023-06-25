#include "menu.h"
#include <math.h>
#include <SDL.h>
#include <nanovg.h>
#include "engine.h"
#include "gui/ugui.h"
#include "scenes/game.h"

float easeOutBack(float x) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}
float easeOutExpo(float x) {
	return x == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
}

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
	menu->vg_font = nvgCreateFont(engine->vg, "PermanentMarker Regular", "res/font/PermanentMarker-Regular.ttf");
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	const struct NVGcontext *vg = engine->vg;

	int mx, my;
	Uint32 mousebtn = SDL_GetMouseState(&mx, &my);

	// upper-right button
	nvgBeginPath(vg);
	nvgRoundedRect(vg, engine->window_width - 70.0f, 8.0f, 62.0f, 62.0f, 5.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, 0.0f, 0.0f, 100.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);
	nvgBeginPath(vg);
	nvgMoveTo(vg, engine->window_width - 58.0f, 24.0f);
	nvgLineTo(vg, engine->window_width - 20.0f, 24.0f);
	nvgMoveTo(vg, engine->window_width - 58.0f, 38.0f);
	nvgLineTo(vg, engine->window_width - 20.0f, 38.0f);
	nvgMoveTo(vg, engine->window_width - 58.0f, 52.0f);
	nvgLineTo(vg, engine->window_width - 20.0f, 52.0f);
	nvgLineCap(vg, NVG_ROUND);
	nvgStrokeWidth(vg, 4.0f);
	nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
	nvgStroke(vg);

	// customize button
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f, engine->window_height - 400.0f, engine->window_width - 100.0f, 75.0f, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 400.0f, 0.0f, engine->window_height - 325.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// play button
	static float playpress_lin = 0.0f;
	const float playpress = easeOutBack(playpress_lin);
	const float playpress2 = easeOutExpo(playpress_lin);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f + 8.0f * playpress2, engine->window_height - 500.0f + 12.0f * playpress, engine->window_width - 100.0f - 16.0f * playpress2, 75.0f - 24.0f * playpress, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 500.0f, 0.0f, engine->window_height - 425.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);
	if (SDL_BUTTON(mousebtn) & 1) {
		if (mx > 50.0f && mx < engine->window_width - 50.0f && my > engine->window_height - 500.0f && my < engine->window_height - 425.0f) {
			playpress_lin += 0.3f;
			if (playpress_lin > 1.0f) {
				playpress_lin = 1.0f;

				struct game_s *game = malloc(sizeof(struct game_s));
				game_init(game, engine, 12, 12);
				engine_setscene(engine, (struct scene_s *)game);
			}
		}
	} else {
		playpress_lin *= 0.85f;
	}

	// play game
	float bounds[4];
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
	nvgFontSize(vg, 32.0f - 4.0f * playpress);
	nvgFontBlur(vg, 4.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Play", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 450.0f, "Play", NULL);
	// shadow
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	nvgFontSize(vg, 32.0f - 4.0f * playpress);
	nvgFontBlur(vg, 0.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Play", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 450.0f, "Play", NULL);

	// customize
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
	nvgFontSize(vg, 32.0f);
	nvgFontBlur(vg, 4.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Customize", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 350.0f, "Customize", NULL);
	// shadow
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	nvgFontSize(vg, 32.0f);
	nvgFontBlur(vg, 0.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Customize", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 350.0f, "Customize", NULL);

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

