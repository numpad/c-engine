#include "menu.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <nanovg.h>
#include "engine.h"
#include "gui/ugui.h"

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
	menu->vg_font = nvgCreateFont(engine->vg, "PermanentMarker Regular", "res/font/PermanentMarker-Regular.ttf");
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	const struct NVGcontext *vg = engine->vg;

	nvgBeginPath(vg);
	nvgRoundedRect(vg, engine->window_width - 70.0f, 8.0f, 62.0f, 62.0f, 5.0f);
	//nvgFillColor(vg, nvgRGBA(140, 125, 225, 200));
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, 0.0f, 0.0f, 100.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// play button
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f, engine->window_height - 300.0f, engine->window_width - 100.0f, 75.0f, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 300.0f, 0.0f, engine->window_height - 225.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// customize
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f, engine->window_height - 400.0f, engine->window_width - 100.0f, 75.0f, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 400.0f, 0.0f, engine->window_height - 325.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// settings
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 50.0f, engine->window_height - 500.0f, engine->window_width - 100.0f, 75.0f, 3.0f);
	nvgFillPaint(vg, nvgLinearGradient(vg, 0.0f, engine->window_height - 500.0f, 0.0f, engine->window_height - 425.0f, nvgRGBA(180, 165, 245, 200), nvgRGBA(110, 105, 200, 200)));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(115, 100, 200, 255));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// play game
	float bounds[4];
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
	nvgFontSize(vg, 32.0f);
	nvgFontBlur(vg, 4.0f);
	nvgTextBounds(vg, 0.0f, 0.0f, "Play", NULL, &bounds);
	nvgText(vg, engine->window_width * 0.5f - bounds[2] * 0.5f, engine->window_height - 450.0f, "Play", NULL);
	// shadow
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
	nvgFontSize(vg, 32.0f);
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

