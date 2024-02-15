#include "background.h"

#include <assert.h>
#include <cglm/cglm.h>
#include <stb_ds.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/shader.h"
#include "gl/vbuffer.h"

//
// structs & enums
//

// 
// private functions
//

static void init_vbuffer_rect(vbuffer_t *vbuf, shader_t *shader);

//
// vars
//

static shader_t g_shader = {0};
static vbuffer_t g_vbuffer = {0};
static texture_t *g_textures = NULL;
static float g_parallax_offset_y = 0.0f;
static int g_textures_len = 0;

//
// public api
//

void background_set_parallax(const char *filename_fmt, int layers_count) {
	assert(g_textures == NULL);

	g_parallax_offset_y = 0.0f;

	// load shader
	shader_init_from_dir(&g_shader, "res/shader/background/");
	assert(g_shader.program != 0);
	
	// load vertices
	init_vbuffer_rect(&g_vbuffer, &g_shader);
	
	// load images
	stbds_arrinsn(g_textures, 0, layers_count);
	struct texture_settings_s settings = {
		.filter_min = GL_LINEAR,
		.filter_mag = GL_NEAREST,
		.wrap_s = GL_REPEAT,
		.wrap_t = GL_CLAMP_TO_EDGE,
		.flip_y = 1,
		.gen_mipmap = 0,
	};

	for (int i = 0; i < layers_count; ++i) {
		char filename[512] = {0};
		snprintf(filename, 512, filename_fmt, i);
		texture_init_from_image(&g_textures[i], filename, &settings);
	}
	g_textures_len = stbds_arrlen(g_textures);
}

void background_set_parallax_offset(float y) {
	g_parallax_offset_y = y;
}

void background_destroy(void) {
	assert(g_shader.program != 0);

	shader_destroy(&g_shader);
	g_shader.program = 0;
	vbuffer_destroy(&g_vbuffer);
	for (int i = 0; i < g_textures_len; ++i) {
		texture_destroy(&g_textures[i]);
	}
	stbds_arrfree(g_textures);
	g_textures = NULL;
	g_textures_len = 0;
}

void background_draw(struct engine_s *engine) {
	if (g_shader.program == 0) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(g_shader.program);
	shader_set_uniform_vec2(&g_shader, "u_resolution", (vec2){ engine->window_width * engine->window_pixel_ratio, engine->window_height * engine->window_pixel_ratio });
	for (int i = g_textures_len - 1; i >= 0; --i) {
		const float p = (g_textures_len - i + 1);
		vec3 parallax_offset = {
			fmodf((engine->time_elapsed * 0.01f) * p, 1.0f),
			g_parallax_offset_y * (0.4f + (1.0f - glm_ease_bounce_in((float)i / g_textures_len)) * 0.5f) - (1.0f - p) * 0.02f,
			(float)i / g_textures_len,
		};
		shader_set_uniform_vec3(&g_shader, "u_parallax_offset", parallax_offset);
		shader_set_uniform_texture(&g_shader, "u_texture", GL_TEXTURE0, &g_textures[i]);
		vbuffer_draw(&g_vbuffer, 6);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}


//
// private implementations
//

static void init_vbuffer_rect(vbuffer_t *vbuf, shader_t *shader) {
	GLfloat vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		-1.0f,  1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
	};

	vbuffer_init(vbuf);
	vbuffer_set_data(vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(vbuf, shader, "a_position", 2, GL_FLOAT, 0, 0);
}

