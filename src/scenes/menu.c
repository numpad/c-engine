#include "menu.h"
#include <stdint.h>
#include <math.h>
#include <SDL.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <cglm/cglm.h>
#include <stb_ds.h>
#include <nuklear.h>
#include "engine.h"
#include "scenes/scene_battle.h"
#include "scenes/experiments.h"
#include "game/isoterrain.h"
#include "game/background.h"
#include "gui/ugui.h"
#include "net/message.h"
#include "platform.h"

//
// vars
//

static int g_font = -1;
static int g_menuicon_play = -1;
static int g_menuicon_cards = -1;
static int g_menuicon_social = -1;

static int others_in_lobby = 0;
static int id_of_current_lobby = 0;
static int *ids_of_lobbies = NULL;
void reset_ids_of_lobbies(void) {
	if (ids_of_lobbies != NULL) {
		stbds_arrfree(ids_of_lobbies);
		ids_of_lobbies = NULL;
	}
}

//
// private functions
//

static void switch_to_game_scene(struct engine_s *engine);
static void switch_to_minigame_scene(struct engine_s *engine);

static void draw_menu(struct engine_s *, struct nk_context *nk);

static float menuitem_active(float menu_x, float bookmark_x, float bookmark_tx);

//
// scene callbacks
//

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	engine->console_visible = 0;

	id_of_current_lobby = 0;
	others_in_lobby = 0;
	reset_ids_of_lobbies();

	if (g_font == -1) g_font = nvgCreateFont(engine->vg, "Baloo", "res/font/Baloo-Regular.ttf");
	if (g_menuicon_play == -1) g_menuicon_play = nvgCreateImage(engine->vg, "res/sprites/menuicon-dice.png", NVG_IMAGE_GENERATE_MIPMAPS);
	if (g_menuicon_cards == -1) g_menuicon_cards = nvgCreateImage(engine->vg, "res/sprites/menuicon-cards.png", NVG_IMAGE_GENERATE_MIPMAPS);
	if (g_menuicon_social == -1) g_menuicon_social = nvgCreateImage(engine->vg, "res/sprites/menuicon-camp.png", NVG_IMAGE_GENERATE_MIPMAPS);

	menu->terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init_from_file(menu->terrain, "res/data/levels/winter.json");
	
	background_set_parallax("res/image/bg-glaciers/%d.png", 4);
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
	isoterrain_destroy(menu->terrain);
	free(menu->terrain);
	background_destroy();
	nvgDeleteImage(engine->vg, g_menuicon_play); g_menuicon_play = -1;
	nvgDeleteImage(engine->vg, g_menuicon_cards); g_menuicon_cards = -1;
	nvgDeleteImage(engine->vg, g_menuicon_social); g_menuicon_social = -1;
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	background_draw(engine);

	// draw terrain
	const float t_padding = 50.0f;
	int t_width;
	isoterrain_get_projected_size(menu->terrain, &t_width, NULL);
	// build matrix
	glm_mat4_identity(engine->u_view);
	const float t_scale = ((engine->window_width - t_padding) / (float)t_width);
	glm_translate(engine->u_view, (vec3){ 25.0f, 160.0f + fabsf(sinf(engine->time_elapsed)) * -30.0f, 0.0f });
	glm_scale(engine->u_view, (vec3){ t_scale, -t_scale, t_scale });
	isoterrain_draw(menu->terrain, engine->u_projection, engine->u_view);

	//draw_menu(engine, engine->nk);

	NVGcontext *vg = engine->vg;
	const float W2 = engine->window_width * 0.5f;
	const float H2 = engine->window_height * 0.5f;

	// draw navbar
	{
		const float bar_height = 60.0f;
		const float menu_width = 110.0f;
		const float menu_pointyness = 30.0f;

		static float bookmark_x = -1.0f;
		static float bookmark_tx = -1.0f;
		if (bookmark_x == -1.0f) {
			bookmark_x = bookmark_tx = W2;
		}
		if (fabsf(bookmark_tx - bookmark_x) > 1.0f) {
			bookmark_x = glm_lerp(bookmark_x, bookmark_tx, 0.2f);
		}

		ugui_mainmenu_bar(engine);
		ugui_mainmenu_bookmark(engine, glm_lerp(bookmark_x, bookmark_tx, 0.5f));
		ugui_mainmenu_icon(engine, W2, "Play", g_menuicon_play, g_font, menuitem_active(W2, bookmark_x, bookmark_tx));
		ugui_mainmenu_icon(engine, W2 - 128.0f, "Cards", g_menuicon_cards, g_font, menuitem_active(W2 - 128.0f, bookmark_x, bookmark_tx));
		ugui_mainmenu_icon(engine, W2 + 128.0f, "Social", g_menuicon_social, g_font, menuitem_active(W2 + 128.0f, bookmark_x, bookmark_tx));

		const NVGcolor green = nvgRGBf(0.80f, 1.0f, 0.42f);
		const NVGcolor green_dark = nvgRGBf(0.39f, 0.82f, 0.20f);
		const NVGcolor red = nvgRGBf(1.0f, 0.5f, 0.35f);
		const NVGcolor red_dark = nvgRGBf(0.79f, 0.3f, 0.16f);
		const NVGcolor yellow = nvgRGBf(1.0f, 0.8f, 0.35f);
		const NVGcolor yellow_dark = nvgRGBf(0.79f, 0.59f, 0.16f);
		ugui_mainmenu_button(engine, 20.0f, H2,             W2 - 40.0f, H2 * 0.5f - 20.0f, "",      "Settings", "",               g_font, red,    red_dark,    nvgRGBf(0.2f, 0.0f, 0.0f));
		ugui_mainmenu_button(engine, 20.0f, H2 + H2 * 0.5f, W2 - 40.0f, H2 * 0.5f - 20.0f, "",      "Shop",     "",               g_font, yellow, yellow_dark, nvgRGBf(0.2f, 0.2f, 0.0f));
		ugui_mainmenu_button(engine, W2,    H2,             W2 - 20.0f, H2 - 20.0f,        "Start", "Game",     "(Singleplayer)", g_font, green,  green_dark,  nvgRGBf(0.0f, 0.2f, 0.0f));

		// logic
		struct input_drag_s *drag = &engine->input_drag;
		if (drag->state == INPUT_DRAG_END && drag->begin_y <= bar_height && drag->end_y <= bar_height) {
			const float p = (drag->end_x / engine->window_width);
			if (p < 0.334f) bookmark_tx = W2 - 128.0f;
			else if (p < 0.667f) bookmark_tx = W2;
			else  bookmark_tx = W2 + 128.0f;
		}
	}

}

static void menu_on_message(struct menu_s *menu, struct engine_s *engine, struct message_header *msg) {
	switch (msg->type) {
		case LOBBY_CREATE_RESPONSE: {
			struct lobby_create_response *response = (struct lobby_create_response *)msg;
			if (response->create_error) {
				break;
			}
			int exists = 0;
			const int lobbies_len = stbds_arrlen(ids_of_lobbies);
			for (int i = 0; i < lobbies_len; ++i) {
				if (ids_of_lobbies[i] == response->lobby_id) {
					exists = 1;
					break;
				}
			}
			if (!exists) {
				stbds_arrpush(ids_of_lobbies, response->lobby_id);
			}
			break;
		}
		case LOBBY_JOIN_RESPONSE: {
			struct lobby_join_response *response = (struct lobby_join_response *)msg;
			if (response->join_error) {
				printf("Failed joining into lobby #%d...\n", response->lobby_id);
				break;
			}

			if (response->is_other_user == 0) {
				printf("Joined lobby #%d!\n", response->lobby_id);
				id_of_current_lobby = response->lobby_id;
				break;
			}

			if (response->is_other_user) {
				printf("Other user joined/left\n");
				others_in_lobby += (response->lobby_id > 0 ? 1 : -1);
			}
			break;
		}
		case LOBBY_LIST_RESPONSE: {
			reset_ids_of_lobbies();

			struct lobby_list_response *response = (struct lobby_list_response *)msg;
			for (int i = 0; i < response->ids_of_lobbies_len; ++i) {
				stbds_arrpush(ids_of_lobbies, response->ids_of_lobbies[i]);
			}
			break;
		}
		case MSG_TYPE_UNKNOWN:
		case MSG_TYPE_MAX:
		case LOBBY_CREATE_REQUEST:
		case LOBBY_JOIN_REQUEST:
		case LOBBY_LIST_REQUEST:
			fprintf(stderr, "Can't handle message %s...\n", message_type_to_name(msg->type));
			break;
	}
}

void scene_menu_init(struct menu_s *menu, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)menu, engine);

	// init function pointers
	menu->base.load       = (scene_load_fn)menu_load;
	menu->base.destroy    = (scene_destroy_fn)menu_destroy;
	menu->base.update     = (scene_update_fn)menu_update;
	menu->base.draw       = (scene_draw_fn)menu_draw;
	menu->base.on_message = (scene_on_message_fn)menu_on_message;
}


//
// private impls
//

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

static void draw_menu(struct engine_s *engine, struct nk_context *nk) {
	const float padding_bottom = 80.0f;
	const float padding_x = 40.0f;
	const float menu_height = 270.0f;
	const int row_height = 55;

	static int multiplayer_window = 0;

	if (nk_begin_titled(nk, "Main Menu", "Main Menu", nk_rect( padding_x, engine->window_height - menu_height - padding_bottom, engine->window_width - padding_x * 2.0f, menu_height), NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
		// play game
		nk_style_push_color(nk, &nk->style.button.text_normal, nk_rgb(60, 170, 30));
		nk_style_push_color(nk, &nk->style.button.text_hover, nk_rgb(50, 140, 10));

		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_TRIANGLE_RIGHT, "Play Game", NK_TEXT_ALIGN_RIGHT)) {
			platform_vibrate(PLATFORM_VIBRATE_TAP);
			switch_to_game_scene(engine);
		}

		nk_style_pop_color(nk);
		nk_style_pop_color(nk);

		// multiplayer
		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_CIRCLE_OUTLINE, "Multiplayer", NK_TEXT_ALIGN_RIGHT)) {
			multiplayer_window = 1;
			platform_vibrate(PLATFORM_VIBRATE_TAP);
			nk_window_show(nk, "Multiplayer", NK_SHOWN);
		}

		// minigame
		nk_layout_row_dynamic(nk, row_height, 1);
		if (nk_button_symbol_label(nk, NK_SYMBOL_PLUS, "Minigame", NK_TEXT_ALIGN_RIGHT)) {
			platform_vibrate(PLATFORM_VIBRATE_TAP);
			switch_to_minigame_scene(engine);
		}


		// settings, about
		nk_layout_row_dynamic(nk, row_height, 2);
		if (nk_button_label(nk, "Settings")) {
			platform_vibrate(PLATFORM_VIBRATE_TAP);
		}
		if (nk_button_label(nk, "About")) {
			platform_vibrate(PLATFORM_VIBRATE_TAP);
			platform_open_website("https://xn--schl-noa.com/");
		}
	}
	nk_end(nk);

	if (multiplayer_window) {
		const int cx = engine->window_width / 2;
		const int cy = engine->window_height / 2;
		const int w = 280, h = 300;
		// state
		static const char *server_ips[] = {"-- Offline --", "gameserver.xn--schl-noa.com", "localhost.schäl.com", "files.xn--schl-noa.com", "192.168.0.17"};
		static int server_ips_i = 0;
		const int is_connected = (engine->gameserver_tcp != NULL);

		if (nk_begin_titled(nk, "Multiplayer", "Multiplayer", nk_rect(cx - w * 0.5f, cy - h * 0.5f, w, h), NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_CLOSABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
			nk_layout_row_dynamic(nk, row_height * 0.5f, 2 + (is_connected ? 1 : 0));
			nk_label(nk, "Server:", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_LEFT);

			// connection status
			if (is_connected) {
				nk_label(nk, server_ips[server_ips_i], NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_LEFT);
				if (nk_button_label(nk, "Disconnect")) {
					engine_gameserver_disconnect(engine);
					server_ips_i = 0;
					id_of_current_lobby = 0;
					others_in_lobby = 0;
					reset_ids_of_lobbies();
				}
			} else {
				const int new_server_ips_i = nk_combo(nk, server_ips, 5, server_ips_i, 30.0f, nk_vec2(200.0f, 200.0f));
				if (new_server_ips_i != server_ips_i) {
					server_ips_i = new_server_ips_i;

					engine_gameserver_connect(engine, server_ips[server_ips_i]);
				}
			}

			// status "not connected"
			if (server_ips_i > 0 && !is_connected) {
				nk_layout_row_dynamic(nk, row_height * 0.5f, 1);
				nk_label(nk, "Not connected...", NK_TEXT_ALIGN_MIDDLE);
			}

			// toolbar
			if (is_connected) {
				// refresh
				nk_layout_row_dynamic(nk, row_height * 0.5f, 3);
				if (nk_button_label(nk, "Refresh")) {
					printf("Refresh requested...\n");
					reset_ids_of_lobbies();

					struct lobby_list_request req;
					message_header_init((struct message_header *)&req, LOBBY_LIST_REQUEST);
					engine_gameserver_send(engine, (struct message_header *)&req);
				}

				nk_label(nk, "", 0);

				// create or leave
				if (id_of_current_lobby > 0) {
				} else {
					if (nk_button_label(nk, "Create Lobby")) {
						struct lobby_create_request req;
						message_header_init(&req.header, LOBBY_CREATE_REQUEST);
						req.lobby_id = rand() % 950 + 17;
						req.lobby_name = "Created Lobby";
						engine_gameserver_send(engine, (struct message_header *)&req);
					}
				}
			}

			// list lobbies
			if (is_connected && id_of_current_lobby == 0) {
				nk_layout_row_dynamic(nk, row_height * 0.5f, 1);
				nk_label(nk, "Server Browser:", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);

				int lobbies_len = stbds_arrlen(ids_of_lobbies);
				if (lobbies_len == 0) {
					nk_layout_row_dynamic(nk, row_height * 0.8f, 1);
					nk_label_colored(nk, "No lobbies found :(", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_CENTERED, nk_rgb(88, 88, 88));
				}
				for (int i = 0; i < lobbies_len; ++i) {
					nk_layout_row_dynamic(nk, row_height * 0.5f, 2);
					nk_labelf(nk, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, "#%d", ids_of_lobbies[i]);

					if (nk_button_label(nk, "Join")) {
						platform_vibrate(PLATFORM_VIBRATE_TAP);

						struct lobby_join_request req;
						message_header_init(&req.header, LOBBY_JOIN_REQUEST);
						req.lobby_id = ids_of_lobbies[i];
						engine_gameserver_send(engine, (struct message_header *)&req);
					}
				}
			}

			// show current lobby
			if (is_connected && id_of_current_lobby > 0) {
				// title
				nk_layout_row_dynamic(nk, row_height * 0.5f, 1);
				nk_labelf(nk, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM, "Lobby #%d", id_of_current_lobby);

				// player list
				nk_layout_row_dynamic(nk, row_height * 0.4f, 1);
				nk_label(nk, " * You", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
				for (int i = 0; i < others_in_lobby; ++i) {
					nk_layout_row_dynamic(nk, row_height * 0.4f, 1);
					nk_label(nk, " * Other user", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
				}
				nk_layout_row_dynamic(nk, row_height * 0.4f, 1);
				nk_label_colored(nk, " * Waiting for others...", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(88, 88, 88));

				// leave
				nk_layout_row_dynamic(nk, row_height * 0.5f, 2);
				nk_label(nk, "", 0);
				if (nk_button_label(nk, "Leave Lobby")) {
					struct lobby_join_request req;
					message_header_init(&req.header, LOBBY_JOIN_REQUEST);
					req.lobby_id = 0;
					others_in_lobby = 0;
					engine_gameserver_send(engine, (struct message_header *)&req);
				}
			}
		}

		if (multiplayer_window && nk_window_is_closed(nk, "Multiplayer")) {
			multiplayer_window = 0;
		}
		nk_end(nk);
	}
}

static float menuitem_active(float menu_x, float bookmark_x, float bookmark_tx) {
	const float d = fabsf(menu_x - bookmark_x);
	const float max_d = 110.0f;
	if (d > max_d || fabs(bookmark_tx - menu_x) > 4.0f) return 0.0f;
	return 1.0f - (d / max_d);
}

