#include "menu.h"
#include <math.h>
#include <SDL.h>
#include <SDL_net.h>
#include <SDL_mixer.h>
#include <nanovg.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stb_ds.h>
#include "engine.h"
#include "gui/console.h"
#include "scenes/battle.h"
#include "scenes/experiments.h"
#include "scenes/planes.h"
#include "scenes/brickbreaker.h"
#include "game/isoterrain.h"
#include "game/background.h"
#include "gui/ugui.h"
#include "gl/texture.h"
#include "gl/graphics2d.h"
#include "net/message.h"
#include "util/util.h"
#include "server/errors.h"
#include "platform.h"

//
// structs & enums
//

typedef void(*btn_callback_fn)(struct engine *engine);
typedef struct {
	float x, y, w, h;
	const char *text1, *text2, *subtext;
	NVGcolor bg1, bg2, outline;
	int id;
	
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
static struct isoterrain_s g_terrain;
static shader_t g_shader_entities;
static pipeline_t g_pipeline_entities;
static int g_minigame_selection_visible = 0;
static int g_minigame_covers = -1;


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
static void switch_to_game_scene(struct engine *engine);
static void switch_to_suika_scene(struct engine *engine);
static void switch_to_asteroid_scene(struct engine *engine);

// button callbacks
static void on_start_game(struct engine *engine);
static void on_click_minigame_button(struct engine *engine);

static void on_begin_search_friends(struct engine *);
static void on_end_search_friends(struct engine *);
static void on_create_lobby(struct engine *);
static void on_join_lobby(struct engine *);

//
// scene callbacks
//

static void load(struct scene_menu_s *menu, struct engine *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);

	g_menu_camera = GLMS_VEC2_ZERO;
	g_menu_camera_target_y = 0.0f;
	g_minigame_selection_visible = 0;

	g_search_friends_text = g_search_friends_texts[0];
	id_of_current_lobby = 0;
	others_in_lobby = 0;
	reset_ids_of_lobbies();

	// load font & icons
	if (g_font == -1) g_font = nvgCreateFont(engine->vg, "Baloo", "res/font/Baloo-Regular.ttf");
	if (g_menuicon_play == -1) g_menuicon_play = nvgCreateImage(engine->vg, "res/sprites/menuicon-dice.png", NVG_IMAGE_GENERATE_MIPMAPS | NVG_IMAGE_PREMULTIPLIED);
	if (g_menuicon_cards == -1) g_menuicon_cards = nvgCreateImage(engine->vg, "res/sprites/menuicon-cards.png", NVG_IMAGE_GENERATE_MIPMAPS | NVG_IMAGE_PREMULTIPLIED);
	if (g_menuicon_social == -1) g_menuicon_social = nvgCreateImage(engine->vg, "res/sprites/menuicon-camp.png", NVG_IMAGE_GENERATE_MIPMAPS | NVG_IMAGE_PREMULTIPLIED);
	if (g_minigame_covers == -1) g_minigame_covers = nvgCreateImage(engine->vg, "res/image/covers/minigames.png", NVG_IMAGE_GENERATE_MIPMAPS);
	// load sfx & music
	if (g_sound_click == NULL) g_sound_click = Mix_LoadWAV("res/sounds/click_002.ogg");
	if (g_sound_clickend == NULL) g_sound_clickend = Mix_LoadWAV("res/sounds/click_005.ogg");
	if (g_music == NULL) g_music = Mix_LoadMUS("res/music/menu-song.ogg");


	isoterrain_init_from_file(&g_terrain, "res/data/levels/winter.json");

	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	texture_init_from_image(&g_entity_tex, "res/sprites/entities-outline.png", &settings);

	shader_init_from_dir(&g_shader_entities, "res/shader/sprite/");
	pipeline_init(&g_pipeline_entities, &g_shader_entities, 128);
	g_pipeline_entities.texture = &g_entity_tex;

	background_set_parallax("res/image/bg-glaciers/%d.png", 5);

	Mix_VolumeMusic(MIX_MAX_VOLUME * 0.2f);
	//Mix_PlayMusic(g_music, -1);

	// setup entities
	g_entities[0].pos[0] = 4;
	g_entities[0].pos[1] = 4;
	g_entities[0].pos[2] = 1;
	g_entities[0].tile[0] = 0;
	g_entities[0].tile[1] = 0;

	g_entities[1].pos[0] = 1;
	g_entities[1].pos[1] = 8;
	g_entities[1].pos[2] = 2;
	g_entities[1].tile[0] = 4;
	g_entities[1].tile[1] = 3;

	g_entities[2].pos[0] = 7;
	g_entities[2].pos[1] = 6;
	g_entities[2].pos[2] = 1;
	g_entities[2].tile[0] = 4;
	g_entities[2].tile[1] = 4;
}

static void destroy(struct scene_menu_s *menu, struct engine *engine) {
	pipeline_destroy(&g_pipeline_entities);
	shader_destroy(&g_shader_entities);
	isoterrain_destroy(&g_terrain);
	background_destroy();
	nvgDeleteImage(engine->vg, g_menuicon_play); g_menuicon_play = -1;
	nvgDeleteImage(engine->vg, g_menuicon_cards); g_menuicon_cards = -1;
	nvgDeleteImage(engine->vg, g_menuicon_social); g_menuicon_social = -1;
	Mix_FreeChunk(g_sound_click); g_sound_click = NULL;
	Mix_FreeChunk(g_sound_clickend); g_sound_clickend = NULL;
	Mix_FreeMusic(g_music); g_music = NULL;
	texture_destroy(&g_entity_tex);
}

static void update(struct scene_menu_s *menu, struct engine *engine, float dt) {
	if (g_next_scene != NULL) {
		engine_setscene(engine, g_next_scene);
		g_next_scene = NULL;
	}
}

static void draw(struct scene_menu_s *menu, struct engine *engine) {
	NVGcontext *vg = engine->vg;
	const float W2 = engine->window_width * 0.5f;
	const float H2 = engine->window_height * 0.5f;

	background_draw(engine);

	// draw terrain
	const float t_padding = 50.0f;
	int t_width = g_terrain.projected_width;

	// build matrix
	glm_mat4_identity(engine->u_view);
	const float t_scale = ((engine->window_width - t_padding) / (float)t_width);
	glm_translate(engine->u_view, (vec3){ t_padding * 0.5f, 180.0f + fabsf(sinf(engine->time_elapsed)) * -30.0f, 0.0f });
	glm_scale(engine->u_view, (vec3){ t_scale, t_scale, t_scale });
	isoterrain_draw(&g_terrain, engine);

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
				.id = 1,
				.text1 = "Settings",
				.bg1 = nvgRGBf(1.0f, 0.5f, 0.35f),
				.bg2 = nvgRGBf(0.79f, 0.3f, 0.16f),
				.outline = nvgRGBf(0.2f, 0.0f, 0.0f),
			},
			{
				20.0f, H2 + H2 * 0.5f + 30.0f, W2 - 40.0f, H2 * 0.5f - 50.0f,
				.id = 2,
				.text1 = "Mini",
				.text2 = "Game",
				.bg1 = nvgRGBf(1.0f, 0.8f, 0.35f),
				.bg2 = nvgRGBf(0.79f, 0.59f, 0.16f),
				.outline = nvgRGBf(0.2f, 0.2f, 0.0f),
				.on_click = &on_click_minigame_button,
			},
			{
				W2, H2 + 60.0f, W2 - 20.0f, H2 - 20.0f - 60.0f,
				.id = 3,
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
				.id = 4,
				.text1 = "Search",
				.text2 = "Friends",
				.subtext = g_search_friends_text,
				.bg1 = nvgRGBf(0.80f, 1.0f, 0.42f),
				.bg2 = nvgRGBf(0.39f, 0.82f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_press_begin = &on_begin_search_friends,
				.on_press_end = &on_end_search_friends,
			},
			{
				W2 - 180.0f + engine->window_width, engine->window_height - 430.0f, 190.0f, 170.0f,
				.id = 5,
				.text1 = "Create",
				.text2 = "Lobby",
				.bg1 = nvgRGBf(0.80f, 0.4f, 0.42f),
				.bg2 = nvgRGBf(0.69f, 0.32f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_click = &on_create_lobby
			},
			{
				W2 + 50.0f + engine->window_width, engine->window_height - 430.0f, 190.0f, 170.0f,
				.id = 6,
				.text1 = "Join",
				.text2 = "Lobby",
				.bg1 = nvgRGBf(0.80f, 0.4f, 0.42f),
				.bg2 = nvgRGBf(0.69f, 0.32f, 0.20f),
				.outline = nvgRGBf(0.0f, 0.2f, 0.0f),
				.on_click = &on_join_lobby
			},
			// start game
			{
				engine->window_width * 0.125f, -150.0f - engine->window_width * 0.125f, engine->window_width * 0.75f, 150.0f,
				.id = 7,
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

		static int active_button_id = 0;
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
					active_button_id = b->id;
					active_button_progress = 0.0f;
					if (b->on_press_begin != NULL) {
						b->on_press_begin(engine);
					}
				}
				if (drag->state == INPUT_DRAG_IN_PROGRESS) {
					active_button_progress += engine->dt;
					active_button_progress = fminf(active_button_progress, 1.0f);
				}
				if (drag->state == INPUT_DRAG_END && b->id == active_button_id) {
					if (b->on_click != NULL && point_in_rect(drag->end_x - g_menu_camera.x, drag->end_y - g_menu_camera.y, b->x, b->y, b->w, b->h)) {
						b->on_click(engine);
					}
					if (b->on_press_end != NULL) {
						b->on_press_end(engine);
					}
					Mix_PlayChannel(-1, g_sound_clickend, 0); // TODO: this gets deleted in menu_destroy and wont play
				}
				if (active_button_id != 0 && drag->state == INPUT_DRAG_NONE) {
					active_button_progress *= 0.83f; // FIXME: framerate independent
					if (active_button_progress <= 0.01f) {
						active_button_progress = 0.0f;
						active_button_id = 0;
					}
				}

				ugui_mainmenu_button(engine, b->x, b->y, b->w, b->h, b->text1, b->text2, b->subtext, g_font, b->bg1, b->bg2, b->outline, (b->id == active_button_id) ? active_button_progress: 0.0f);
			}

			// Minigame Selection Modal Window
			if (g_minigame_selection_visible) {
				rendered_modal_t modal = ugui_modal_begin(engine);

				// Title text
				nvgFontFaceId(vg, g_font);
				nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
				nvgFontSize(vg, 24.0f);
				nvgFillColor(vg, nvgRGBf(0.4f, 0.41f, 0.24f));
				nvgText(vg, 16.0f, 16.0f, "CHOOSE A GAME", NULL);

				// Carousel Item
				{
					const NVGpaint image_paint = nvgImagePattern(vg,
							75.0f, 60.0f, 512.0f, 256.0f,
							0.0f, g_minigame_covers, 1.0f);
					const NVGpaint shine_overlay = nvgRadialGradient(vg,
							125.0f, 125.0f, 10.0f, 250.0f,
							nvgRGBAf(1.0f, 1.0f, 1.0f, 0.03f), nvgRGBAf(1.0f, 1.0f, 1.0f, 0.0f));
					// Cover Image
					nvgBeginPath(vg);
					nvgRoundedRect(vg, 75.0f, 60.0f, 150.0f, 256.0f, 8.0f);
					nvgFillPaint(vg, image_paint);
					nvgFill(vg);
					// Shine Overlay
					nvgBeginPath(vg);
					nvgRoundedRect(vg, 75.0f, 60.0f, 150.0f, 256.0f, 8.0f);
					nvgFillPaint(vg, shine_overlay);
					nvgFill(vg);
					// Outline
					nvgBeginPath(vg);
					nvgRoundedRect(vg, 75.0f, 60.0f, 150.0f, 256.0f, 8.0f);
					nvgStrokeColor(vg, nvgRGBf(0.33f, 0.2f, 0.25f));
					nvgStrokeWidth(vg, 2.5f);
					nvgStroke(vg);

					// Name Box
					nvgBeginPath(vg);
					nvgRoundedRect(vg, 75.0f, 60.0f + 256.0f + 13.0f, 150.0f, 36.0f, 8.0f);
					nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 0.9f));
					nvgFill(vg);

					// Carousel Inner Shadows
					{
						NVGcolor inner = nvgRGBA(0, 0, 0, 100);
						NVGcolor outer = nvgRGBA(0, 0, 0, 0);
						// Body
						NVGpaint gradient_shadow_left = nvgLinearGradient(vg, 0.0f, 0.0f, 20.0f, 0.0f, inner, outer);
						nvgBeginPath(vg);
						nvgRect(vg, 0.0f, 70.0f, 20.0f, modal.height - 2*70.0f);
						nvgFillPaint(vg, gradient_shadow_left);
						nvgFill(vg);

						// Top Edge
						NVGpaint gradient_shadow_topleft = nvgRadialGradient(vg, 0.0f, 50.0f + 20.0f, 0.0f, 20.0f, inner, outer);
						nvgBeginPath(vg);
						nvgRect(vg, 0.0f, 50.0f, 20.0f, 20.0f);
						nvgFillPaint(vg, gradient_shadow_topleft);
						nvgFill(vg);

						// Bottom Edge
						NVGpaint gradient_shadow_bottomleft = nvgRadialGradient(vg, 0.0f, modal.height - 70.0f, 0.0f, 20.0f, inner, outer);
						nvgBeginPath(vg);
						nvgRect(vg, 0.0f, modal.height - 70.0f, 20.0f, 20.0f);
						nvgFillPaint(vg, gradient_shadow_bottomleft);
						nvgFill(vg);
					}
				}

				ugui_modal_end(modal);
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

	// test entity rendering
	static float entity_anim = 0.0f;
	entity_anim = fmodf(entity_anim + engine->dt, 1.0f);

	pipeline_reset(&g_pipeline_entities);
	for (size_t i = 0; i < g_entities_len; ++i) {
		entity_t *e = &g_entities[i];
		vec2s bp = GLMS_VEC2_ZERO_INIT;
		isoterrain_pos_block_to_screen(&g_terrain, e->pos[0], e->pos[1], e->pos[2], bp.raw);
		
		// draw sprite
		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = 16;
		cmd.size.y = 17;
		glm_vec2(bp.raw, cmd.position.raw);
		cmd.position.y = g_terrain.projected_height - cmd.position.y;
		drawcmd_set_texture_subrect_tile(&cmd, &g_entity_tex, 16, 17, e->tile[0] + (entity_anim < 0.5f), e->tile[1]);
		pipeline_emit(&g_pipeline_entities, &cmd);
	}

	pipeline_draw(&g_pipeline_entities, engine);
}

static void on_callback(struct scene_menu_s *menu, struct engine *engine, struct engine_event event) {
	switch (event.type) {
		case ENGINE_EVENT_CLOSE_SCENE:
			g_menu_camera_target_y = 0.0f;
			break;
		case ENGINE_EVENT_WINDOW_RESIZED:
			break;
		case ENGINE_EVENT_KEY:
			break;
		default:
		case ENGINE_EVENT_MAX:
			break;
	};
}

static void on_message(struct scene_menu_s *menu, struct engine *engine, struct message_header *msg) {
	switch (msg->type) {
		case WELCOME_RESPONSE: {
			struct welcome_response *_response = (struct welcome_response *)msg;
			console_log_ex(engine, CONSOLE_MSG_SUCCESS, 4.0f, "Received WELCOME");
			break;
		}
		case LOBBY_CREATE_RESPONSE: {
			struct lobby_create_response *response = (struct lobby_create_response *)msg;
			if (response->create_error != SERVER_NO_ERROR) {
				console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed creating lobby #%d: \"%s\"",
							   response->lobby_id, server_error_description(response->create_error));
				break;
			}

			console_log_ex(engine, CONSOLE_MSG_SUCCESS, 4.0f, "Created lobby #%d", response->lobby_id);
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
				console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed joining lobby #%d: \"%s\"",
							   response->lobby_id, server_error_description(response->join_error));
				break;
			}

			if (response->is_other_user == 0) {
				console_log_ex(engine, CONSOLE_MSG_SUCCESS, 4.0f, "Joined lobby #%d", response->lobby_id);
				id_of_current_lobby = response->lobby_id;
				break;
			}

			if (response->is_other_user) {
				console_log(engine, "Someone %s the lobby.", response->lobby_id > 0 ? "joined" : "left");
				others_in_lobby += (response->lobby_id > 0 ? 1 : -1);
			}
			break;
		}
		case LOBBY_LIST_RESPONSE: {
			reset_ids_of_lobbies();

			struct lobby_list_response *response = (struct lobby_list_response *)msg;
			console_log_ex(engine, CONSOLE_MSG_SUCCESS, 4.0f, "Received LOBBY_LIST_RESPONSE (%d lobbies)", response->ids_of_lobbies_len);

			for (int i = 0; i < response->ids_of_lobbies_len; ++i) {
				stbds_arrpush(ids_of_lobbies, response->ids_of_lobbies[i]);
			}
			break;
		}
		case MSG_DISCONNECTED:
			console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Disconnected...");
			break;
		case MSG_TYPE_UNKNOWN:
		case MSG_TYPE_MAX:
		case LOBBY_CREATE_REQUEST:
		case LOBBY_JOIN_REQUEST:
		case LOBBY_LIST_REQUEST:
			fprintf(stderr, "Can't handle message %s...\n", message_type_to_name(msg->type));
			break;
	}
}

void scene_menu_init(struct scene_menu_s *menu, struct engine *engine) {
	// init scene base
	scene_init((struct scene_s *)menu, engine);

	// init function pointers
	menu->base.load        = (scene_load_fn)load;
	menu->base.destroy     = (scene_destroy_fn)destroy;
	menu->base.update      = (scene_update_fn)update;
	menu->base.draw        = (scene_draw_fn)draw;
	menu->base.on_callback = (scene_on_callback_fn)on_callback;
	menu->base.on_message  = (scene_on_message_fn)on_message;
}


//
// private impls
//

static void on_start_game(struct engine *engine) {
	g_menu_camera_target_y = -1.0f;

	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static void on_click_minigame_button(struct engine *engine) {
	g_minigame_selection_visible = 1;
}

static void switch_to_game_scene(struct engine *engine) {
	struct scene_battle_s *game_scene_battle = malloc(sizeof(struct scene_battle_s));
	scene_battle_init(game_scene_battle, engine);
	g_next_scene = (struct scene_s *)game_scene_battle;

	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static void switch_to_suika_scene(struct engine *engine) {
	struct scene_experiments_s *scene = malloc(sizeof(struct scene_experiments_s));
	scene_experiments_init(scene, engine);
	g_next_scene = (struct scene_s *)scene;


	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static void switch_to_asteroid_scene(struct engine *engine) {
	struct scene_brickbreaker_s *scene = malloc(sizeof(struct scene_brickbreaker_s));
	scene_brickbreaker_init(scene, engine);
	g_next_scene = (struct scene_s *)scene;


	platform_vibrate(PLATFORM_VIBRATE_TAP);
}

static float menuitem_active(float menu_x, float bookmark_x, float bookmark_tx) {
	const float d = fabsf(menu_x - bookmark_x);
	const float max_d = 110.0f;
	if (d > max_d || fabs(bookmark_tx - menu_x) > 4.0f) return 0.0f;
	return 1.0f - (d / max_d);
}

static void on_begin_search_friends(struct engine *engine) {
	g_search_friends_text = g_search_friends_texts[1];

	// TODO: remove
	if (engine->gameserver_tcp == NULL) {
		// TODO: When deployed connect to "gameserver.xn--schl-noa.com". Maybe different URL depending on native/WASM/...?
		if (engine_gameserver_connect(engine, "localhost") == 0) {
			console_log_ex(engine, CONSOLE_MSG_SUCCESS, 4.0f, "Connected to server");
		} else {
			console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed to connect...");
		}
	} else {
		struct lobby_list_request req;
		message_header_init((struct message_header *)&req, LOBBY_LIST_REQUEST);
		engine_gameserver_send(engine, (struct message_header *)&req);
	}
}

static void on_end_search_friends(struct engine *engine) {
	g_search_friends_text = g_search_friends_texts[0];
}

static void on_create_lobby(struct engine *engine) {
	struct lobby_create_request req;
	message_header_init((struct message_header *)&req, LOBBY_CREATE_REQUEST);
	req.lobby_id = 123;
	req.lobby_name = "My Test Lobby";
	engine_gameserver_send(engine, (struct message_header *)&req);
}

static void on_join_lobby(struct engine *engine) {
	struct lobby_join_request req;
	message_header_init((struct message_header *)&req, LOBBY_JOIN_REQUEST);
	req.lobby_id = 123;
	engine_gameserver_send(engine, (struct message_header *)&req);
}

