#include "menu.h"
#include <math.h>
#include <SDL.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <cglm/cglm.h>
#include <nuklear.h>
#include "engine.h"
#include "scenes/scene_battle.h"
#include "scenes/experiments.h"
#include "game/isoterrain.h"

float easeOutExpo(float x) {
	return x == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
}

static void switch_to_game_scene(struct engine_s *engine) {
	struct scene_battle_s *game_scene_battle = malloc(sizeof(struct scene_battle_s));
	scene_battle_init(game_scene_battle, engine);
	engine_setscene(engine, (struct scene_s *)game_scene_battle);
}

static void switch_to_minigame_scene(struct engine_s *engine) {
	struct scene_experiments_s *game_scene_experiments = malloc(sizeof(struct scene_experiments_s));
	scene_experiments_init(game_scene_experiments, engine);
	engine_setscene(engine, (struct scene_s *)game_scene_experiments);
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

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {

	// gui
	struct nk_context *nk = engine->nk;
	const float padding_top = engine->window_height * 0.33f;
	const float padding_x = 30.0f, padding_y = 50.0f;
	const int row_height = 45;

	static int join_lobby_window = 0;

	if (nk_begin_titled(nk, "Main Menu", "Main Menu", nk_rect( padding_x, padding_y + padding_top, engine->window_width - padding_x * 2.0f, engine->window_height - padding_y * 2.0f - padding_top), NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		// play game
		nk_style_push_color(nk, &nk->style.button.text_normal, nk_rgb(60, 170, 30));
		nk_style_push_color(nk, &nk->style.button.text_hover, nk_rgb(50, 140, 10));

		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_TRIANGLE_RIGHT, "Play Game", NK_TEXT_ALIGN_RIGHT)) {
			switch_to_game_scene(engine);
		}

		nk_style_pop_color(nk);
		nk_style_pop_color(nk);

		// continue
		nk_style_push_style_item(nk, &nk->style.button.normal, nk_style_item_color(nk_rgb(66, 66, 66)));
		nk_style_push_style_item(nk, &nk->style.button.active, nk->style.button.normal);
		nk_style_push_style_item(nk, &nk->style.button.hover, nk->style.button.normal);
		nk_style_push_color(nk, &nk->style.button.text_normal, nk_rgb(120, 120, 120));
		nk_style_push_color(nk, &nk->style.button.text_active, nk->style.button.text_normal);
		nk_style_push_color(nk, &nk->style.button.text_hover, nk->style.button.text_normal);

		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_TRIANGLE_RIGHT, "Continue", NK_TEXT_ALIGN_RIGHT)) {
		}

		nk_style_pop_style_item(nk);
		nk_style_pop_style_item(nk);
		nk_style_pop_style_item(nk);
		nk_style_pop_color(nk);
		nk_style_pop_color(nk);
		nk_style_pop_color(nk);

		// multiplayer
		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_CIRCLE_OUTLINE, "Join Lobby", NK_TEXT_ALIGN_RIGHT)) {
			join_lobby_window = 1;
			nk_window_show(nk, "Join Game", NK_SHOWN);
		}

		// minigame
		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_PLUS, "Minigame", NK_TEXT_ALIGN_RIGHT)) {
			switch_to_minigame_scene(engine);
		}


		// settings, about
		nk_layout_row_dynamic(nk, row_height, 2);
		if (nk_button_label(nk, "Settings")) {
		}
		if (nk_button_label(nk, "About")) {
		}
	}
	nk_end(nk);

	if (join_lobby_window) {
		const int cx = engine->window_width / 2;
		const int cy = engine->window_height / 2;
		const int w = 340, h = 300;
		static char *error_message = NULL;
		static struct nk_color error_message_color;
		const struct nk_color color_error = nk_rgb(255, 50, 50);
		const struct nk_color color_success = nk_rgb(50, 255, 50);
		const struct nk_color color_warning = nk_rgb(255, 255, 50);

		if (nk_begin_titled(nk, "Join Game", "Join Game", nk_rect(cx - w * 0.5f, cy - h * 0.5f, w, h), NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_CLOSABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
			nk_layout_row_dynamic(nk, row_height * 0.5f, 1);
			nk_label(nk, "Host Address:", NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_LEFT);

			static char input_host[128] = "schäl.com";
			nk_layout_row_dynamic(nk, row_height, 1);
			nk_edit_string_zero_terminated(nk, NK_EDIT_FIELD | NK_EDIT_SELECTABLE, input_host, sizeof(input_host) - 1, nk_filter_default);

			if (engine->gameserver_tcp == NULL) {
				nk_layout_row_dynamic(nk, row_height, 1);
				if (nk_button_label(nk, "Join")) {
					error_message = "Connecting...";
					error_message_color = color_warning;

					do {
						if (engine_gameserver_connect(engine, input_host) != 0) {
							error_message = "Connecting failed...";
							error_message_color = color_error;
							break;
						}
						error_message = "Socket open!";
						error_message_color = color_success;

						const int result = SDLNet_TCP_Send(engine->gameserver_tcp, "hello!", 8);
						if (result < 8) {
							error_message = "Socket open, but failed sending immediately...";
							error_message_color = color_warning;
							break;
						}

					} while (0);

				}
			}

			if (engine->gameserver_tcp != NULL) {
				nk_layout_row_dynamic(nk, row_height, 1);
				if (nk_button_label(nk, "Send data")) {
					const int result = SDLNet_TCP_Send(engine->gameserver_tcp, "random msg", 11);
					if (result < 11) {
						error_message = "Failed sending...";
						error_message_color = color_warning;
					} else {
						error_message = "Data sent!";
						error_message_color = color_success;
					}
				}
			}

			if (error_message != NULL) {
				nk_layout_row_dynamic(nk, row_height * 0.5f, 1);
				nk_label_colored(nk, error_message, NK_TEXT_ALIGN_BOTTOM | NK_TEXT_ALIGN_LEFT, error_message_color);
			}
		}

		if (join_lobby_window && nk_window_is_closed(nk, "Join Game")) {
			join_lobby_window = 0;
		}
		nk_end(nk);
	}
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	struct NVGcontext *vg = engine->vg;

	int mx, my;
	Uint32 mousebtn = SDL_GetMouseState(&mx, &my);

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

