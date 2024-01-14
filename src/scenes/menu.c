#include "menu.h"
#include <stdint.h>
#include <math.h>
#include <SDL.h>
#include <SDL_net.h>
#include <SDL_mixer.h>
#include <nanovg.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stb_ds.h>
#include <nuklear.h>
#include "engine.h"
#include "scenes/scene_battle.h"
#include "scenes/experiments.h"
#include "game/isoterrain.h"
#include "game/background.h"
#include "gui/ugui.h"
#include "gl/texture.h"
#include "gl/graphics2d.h"
#include "net/message.h"
#include "util/util.h"
#include "platform.h"

//
// structs & enums
//

typedef void(*btn_callback_fn)(struct engine_s *engine);
typedef struct {
	float x, y, w, h;
	const char *text1, *text2, *subtext;
	NVGcolor bg1, bg2, outline;
	
	btn_callback_fn on_press_begin;
	btn_callback_fn on_press_end;
	btn_callback_fn on_click;
} mainbutton_t;

typedef struct {
	float x;
	const char *text;
	int icon;
} navitem_t;

typedef struct {
	ivec3 pos;
	ivec2 tile;
} entity_t;

//
// vars
//

// gui
static int g_font = -1;
static int g_menuicon_play = -1;
static int g_menuicon_cards = -1;
static int g_menuicon_social = -1;
static texture_t g_entity_tex;
static const char *g_search_friends_texts[] = {"(Both Users Hold Button)", "Searching..."};
static const char *g_search_friends_text = NULL;
static vec2s g_menu_camera;
static float g_menu_camera_target_y = 0.0f;

// sfx
static Mix_Chunk *g_sound_click = NULL;
static Mix_Chunk *g_sound_clickend = NULL;
static Mix_Music *g_music = NULL;

static size_t g_entities_len = 3;
static entity_t g_entities[3];

struct scene_s *g_next_scene = NULL;

// multiplayer
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

// helper
static float menuitem_active(float menu_x, float bookmark_x, float bookmark_tx);

// switch scenes
static void switch_to_game_scene(struct engine_s *engine);
static void switch_to_minigame_scene(struct engine_s *engine);

// button callbacks
static void on_start_game(struct engine_s *engine);
void on_begin_search_friends(struct engine_s *);
void on_end_search_friends(struct engine_s *);

// deprecated
static void draw_menu(struct engine_s *, struct nk_context *nk);

//
// scene callbacks
//

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	engine->console_visible = 0;

	g_menu_camera = GLMS_VEC2_ZERO;
	g_menu_camera_target_y = 0.0f;

	g_search_friends_text = g_search_friends_texts[0];
	id_of_current_lobby = 0;
	others_in_lobby = 0;
	reset_ids_of_lobbies();

	// load font & icons
	if (g_font == -1) g_font = nvgCreateFont(engine->vg, "Baloo", "res/font/Baloo-Regular.ttf");
	if (g_menuicon_play == -1) g_menuicon_play = nvgCreateImage(engine->vg, "res/sprites/menuicon-dice.png", NVG_IMAGE_GENERATE_MIPMAPS);
	if (g_menuicon_cards == -1) g_menuicon_cards = nvgCreateImage(engine->vg, "res/sprites/menuicon-cards.png", NVG_IMAGE_GENERATE_MIPMAPS);
	if (g_menuicon_social == -1) g_menuicon_social = nvgCreateImage(engine->vg, "res/sprites/menuicon-camp.png", NVG_IMAGE_GENERATE_MIPMAPS);
	// load sfx & music
	if (g_sound_click == NULL) g_sound_click = Mix_LoadWAV("res/sounds/click_002.ogg");
	if (g_sound_clickend == NULL) g_sound_clickend = Mix_LoadWAV("res/sounds/click_005.ogg");
	if (g_music == NULL) g_music = Mix_LoadMUS("res/music/menu-song.ogg");


	menu->terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init_from_file(menu->terrain, "res/data/levels/winter.json");

	texture_init_from_image(&g_entity_tex, "res/sprites/entities-outline.png", NULL);

	background_set_parallax("res/image/bg-glaciers/%d.png", 4);

	Mix_VolumeMusic(MIX_MAX_VOLUME * 0.2f);
	//Mix_PlayMusic(g_music, -1);

	// setup entities
	g_entities[0].pos[0] = 4;
	g_entities[0].pos[1] = 4;
	g_entities[0].pos[2] = 1;
	g_entities[0].tile[0] = 0;
	g_entities[0].tile[1] = 1;

	g_entities[1].pos[0] = 1;
	g_entities[1].pos[1] = 8;
	g_entities[1].pos[2] = 2;
	g_entities[1].tile[0] = 4;
	g_entities[1].tile[1] = 7;

	g_entities[2].pos[0] = 7;
	g_entities[2].pos[1] = 6;
	g_entities[2].pos[2] = 1;
	g_entities[2].tile[0] = 12;
	g_entities[2].tile[1] = 7;
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
	isoterrain_destroy(menu->terrain);
	free(menu->terrain);
	background_destroy();
	nvgDeleteImage(engine->vg, g_menuicon_play); g_menuicon_play = -1;
	nvgDeleteImage(engine->vg, g_menuicon_cards); g_menuicon_cards = -1;
	nvgDeleteImage(engine->vg, g_menuicon_social); g_menuicon_social = -1;
	Mix_FreeChunk(g_sound_click); g_sound_click = NULL;
	Mix_FreeChunk(g_sound_clickend); g_sound_clickend = NULL;
	Mix_FreeMusic(g_music); g_music = NULL;
	texture_destroy(&g_entity_tex);
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
	if (g_next_scene != NULL) {
		engine_setscene(engine, g_next_scene);
		g_next_scene = NULL;
	}
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;
	const float W2 = engine->window_width * 0.5f;
	const float H2 = engine->window_height * 0.5f;

	background_draw(engine);

	// draw terrain
	const float t_padding = 50.0f;
	int t_width = menu->terrain->projected_width;

	// build matrix
	glm_mat4_identity(engine->u_view);
	const float t_scale = ((engine->window_width - t_padding) / (float)t_width);
	glm_translate(engine->u_view, (vec3){ 25.0f + g_menu_camera.x, 160.0f + fabsf(sinf(engine->time_elapsed)) * -30.0f, 0.0f });
	glm_scale(engine->u_view, (vec3){ t_scale, -t_scale, t_scale });
	isoterrain_draw(menu->terrain, engine->u_projection, engine->u_view);

	//draw_menu(engine, engine->nk);

	// draw menu
	{
		struct input_drag_s *drag = &engine->input_drag;
		const float bar_height = 60.0f;

		static float bookmark_x = -1.0f;
		static size_t active_navitem = 1;
		if (bookmark_x == -1.0f) {
			bookmark_x = W2;
		}

		// icons
		navitem_t navitems[] = {
			{ .x = W2 - 128.0f, .text = "Cards", .icon = g_menuicon_cards },
			{ .x = W2         , .text = "Play", .icon = g_menuicon_play },
			{ .x = W2 + 128.0f, .text = "Social", .icon = g_menuicon_social },
			{ .text = NULL }, // "null" item
		};

		// buttons
		mainbutton_t buttons[] = {
			{
				20.0f, H2 + 60.0f, W2 - 40.0f, H2 * 0.5f - 50.0f,
				.text1 = "Settings",
				.bg1 = nvgRGBf(1.0f, 0.5f, 0.35f),
				.bg2 = nvgRGBf(0.79f, 0.3f, 0.16f),
				.outline = nvgRGBf(0.2f, 0.0f, 0.0f),
			},
			{
				20.0f, H2 + H2 * 0.5f + 30.0f, W2 - 40.0f, H2 * 0.5f - 50.0f,
				.text1 = "Mini",
				.text2 = "Game",
				.bg1 = nvgRGBf(1.0f, 0.8f, 0.35f),
				.bg2 = nvgRGBf(0.79f, 0.59f, 0.16f),
				.outline = nvgRGBf(0.2f, 0.2f, 0.0f),
				.on_click = &switch_to_minigame_scene,
			},
			{
				W2, H2 + 60.0f, W2 - 20.0f, H2 - 20.0f - 60.0f,
				.text1 = "Start",
				.text2 = "Game",
				.subtext = "(Singleplayer)",
				.bg1 = nvgRGBf(0.80f, 1.0f, 0.42f),
				.bg2 = nvgRGBf(0.39f, 0.82f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_click = &on_start_game,
			},
			// "social"
			{
				W2 - 120.0f + engine->window_width, engine->window_height - 240.0f, 240.0f, 190.0f,
				.text1 = "Search",
				.text2 = "Friends",
				.subtext = g_search_friends_text,
				.bg1 = nvgRGBf(0.80f, 1.0f, 0.42f),
				.bg2 = nvgRGBf(0.39f, 0.82f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_press_begin = &on_begin_search_friends,
				.on_press_end = &on_end_search_friends,
			},
			// start game
			{
				engine->window_width * 0.125f, -150.0f - engine->window_width * 0.125f, engine->window_width * 0.75f, 150.0f,
				.text1 = "Start →",
				.text2 = NULL,
				.subtext = NULL,
				.bg1 = nvgRGBf(0.80f, 1.0f, 0.42f),
				.bg2 = nvgRGBf(0.39f, 0.82f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_click = &switch_to_game_scene,
			},
			{ .text1 = NULL } // "null" elem
		};

		static mainbutton_t *active_button = NULL;
		static float active_button_progress = 0.0f;

		// draw menu tabs/screens
		g_menu_camera.x = ((bookmark_x - W2) / 128.0f) * -engine->window_width;
		g_menu_camera.y = glm_lerp(g_menu_camera.y, g_menu_camera_target_y * engine->window_height * -1.0f, engine->dt * 5.0f);
		background_set_parallax_offset(-0.4f + g_menu_camera.y / engine->window_height * 0.4f);

		nvgSave(vg);
		nvgTranslate(vg, g_menu_camera.x, g_menu_camera.y);

		// "cards" menu
		{
			// nvgSave(vg);
			// nvgTranslate(vg, -engine->window_width, 0.0f);
			// ugui_mainmenu_button(engine, 10.0f, 300.0f, engine->window_width - 20.0f, 200.0f, "Card", "Screen", "Noch in arbeit :)", g_font, buttons[0].bg1, buttons[0].bg2, buttons[0].outline, 0.0f);
			// nvgRestore(vg);
		}

		// "play" menu
		{
			for (mainbutton_t *b = &buttons[0]; b->text1 != NULL; ++b) {
				if (drag->state == INPUT_DRAG_BEGIN && point_in_rect(drag->begin_x - g_menu_camera.x, drag->begin_y - g_menu_camera.y, b->x, b->y, b->w, b->h)) {
					Mix_PlayChannel(-1, g_sound_click, 0);
					active_button = b;
					active_button_progress = 0.0f;
					if (b->on_press_begin != NULL) {
						b->on_press_begin(engine);
					}
				}
				if (drag->state == INPUT_DRAG_IN_PROGRESS) {
					active_button_progress += engine->dt;
					active_button_progress = fminf(active_button_progress, 1.0f);
				}
				if (drag->state == INPUT_DRAG_END && b == active_button) {
					if (b->on_click != NULL && point_in_rect(drag->end_x - g_menu_camera.x, drag->end_y - g_menu_camera.y, b->x, b->y, b->w, b->h)) {
						b->on_click(engine);
					}
					if (b->on_press_end != NULL) {
						b->on_press_end(engine);
					}
					Mix_PlayChannel(-1, g_sound_clickend, 0); // TODO: this gets deleted in menu_destroy and wont play
				}
				if (active_button != NULL && drag->state == INPUT_DRAG_NONE) {
					active_button_progress *= 0.83f; // FIXME: framerate independent
					if (active_button_progress < 0.01f) {
						active_button_progress = 0.0f;
						active_button = NULL;
					}
				}

				ugui_mainmenu_button(engine, b->x, b->y, b->w, b->h, b->text1, b->text2, b->subtext, g_font, b->bg1, b->bg2, b->outline, (b == active_button) ? active_button_progress: 0.0f);
			}
		}

		// "social" menu
		{
			nvgSave(vg);
			nvgTranslate(vg, engine->window_width, 0.0f);

			ugui_mainmenu_checkbox(engine, 50.0f, 110.0f, 90.0f, 45.0f, g_font, "Multiplayer", sinf(engine->time_elapsed * 3.0f) * 0.5f + 0.5f);

			// draw here...
			nvgRestore(vg);
		}

		nvgRestore(vg);

		// draw navbar
		ugui_mainmenu_bar(engine);
		ugui_mainmenu_bookmark(engine, bookmark_x);
		for (navitem_t *i = &navitems[0]; i->text != NULL; ++i) {
			ugui_mainmenu_icon(engine, i->x, i->text, i->icon, g_font, menuitem_active(i->x, bookmark_x, navitems[active_navitem].x));
		}

		// logic
		if (fabsf(navitems[active_navitem].x - bookmark_x) > 0.001f) {
			bookmark_x = glm_lerp(bookmark_x, navitems[active_navitem].x, engine->dt * 15.0f);
		}
		if (drag->state == INPUT_DRAG_END && drag->begin_y <= bar_height && drag->end_y <= bar_height) {
			size_t index = 0;
			for (navitem_t *i = &navitems[0]; i->text != NULL; ++i) {
				if (fabsf(drag->end_x - i->x) < 68.0f) {
					if (index != active_navitem) {
						Mix_PlayChannel(-1, g_sound_click, 0);
						active_navitem = index;
						g_menu_camera_target_y = 0.0f;
					}
					break;
				}
				++index;
			}
		}
	}

	static float entity_anim = 0.0f;
	entity_anim = fmodf(entity_anim + engine->dt, 1.0f);

	// test entity rendering
	primitive2d_t sprite = {0};
	graphics2d_init_rect(&sprite, 0.0f, 0.0f, 16.0f, 17.0f);
	for (size_t i = 0; i < g_entities_len; ++i) {
		entity_t *e = &g_entities[i];
		vec2s bp = GLMS_VEC2_ZERO_INIT;
		isoterrain_pos_block_to_screen(menu->terrain, e->pos[0], e->pos[1], e->pos[2], bp.raw);
		
		// draw sprite
		graphics2d_set_position(&sprite, bp.x, bp.y);
		graphics2d_set_texture_tile(&sprite, &g_entity_tex, 16.0f, 17.0f, e->tile[0] + (entity_anim < 0.5f), e->tile[1]);
		graphics2d_draw_primitive2d(engine, &sprite);
	}
}

static void menu_on_message(struct menu_s *menu, struct engine_s *engine, struct message_header *msg) {
	switch (msg->type) {
		case WELCOME_RESPONSE: {
			struct welcome_response *response = (struct welcome_response *)msg;
			break;
		}
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

static void on_start_game(struct engine_s *engine) {
	g_menu_camera_target_y = -1.0f;

	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static void switch_to_game_scene(struct engine_s *engine) {
	struct scene_battle_s *game_scene_battle = malloc(sizeof(struct scene_battle_s));
	scene_battle_init(game_scene_battle, engine);
	g_next_scene = (struct scene_s *)game_scene_battle;

	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static void switch_to_minigame_scene(struct engine_s *engine) {
	struct scene_experiments_s *game_scene_experiments = malloc(sizeof(struct scene_experiments_s));
	scene_experiments_init(game_scene_experiments, engine);
	//engine_setscene(engine, (struct scene_s *)game_scene_experiments);
	g_next_scene = (struct scene_s *)game_scene_experiments;

	platform_vibrate(PLATFORM_VIBRATE_TAP);
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
			on_start_game(engine);
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

void on_begin_search_friends(struct engine_s *engine) {
	g_search_friends_text = g_search_friends_texts[1];
}

void on_end_search_friends(struct engine_s *engine) {
	g_search_friends_text = g_search_friends_texts[0];
}

