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

//
// public api
//

void background_set_image(const char *filename) {
	assert(g_textures == NULL);

	// load shader
	shader_init_from_dir(&g_shader, "res/shader/background/");
	assert(g_shader.program != 0);

	// load vertices
	init_vbuffer_rect(&g_vbuffer, &g_shader);

	// load image
	stbds_arrinsn(g_textures, 0, 1);
	texture_init_from_image(&g_textures[0], filename, NULL);
	
}

void background_set_parallax(const char *filename_fmt, int layers_count) {
	assert(g_textures == NULL);

	// load shader
	shader_init_from_dir(&g_shader, "res/shader/background/");
	assert(g_shader.program != 0);
	
	// load vertices
	init_vbuffer_rect(&g_vbuffer, &g_shader);
	
	// load images
	stbds_arrinsn(g_textures, 0, layers_count);
	for (int i = 0; i < layers_count; ++i) {
		char filename[512] = {0};
		snprintf(filename, 512, filename_fmt, i);
		texture_init_from_image(&g_textures[i], filename, NULL);
	}
}

void background_destroy(void) {
	assert(g_shader.program != 0);

	shader_destroy(&g_shader);
	g_shader.program = 0;
	vbuffer_destroy(&g_vbuffer);
	for (int i = 0; i < stbds_arrlen(g_textures); ++i) {
		texture_destroy(&g_textures[i]);
	}
	stbds_arrfree(g_textures);
	g_textures = NULL;
}

void background_draw(struct engine_s *engine) {
	if (g_shader.program == 0) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(g_shader.program);
	shader_set_uniform_vec2(&g_shader, "u_resolution", (vec2){ engine->window_width * engine->window_pixel_ratio, engine->window_height * engine->window_pixel_ratio });
	const int g_textures_len = stbds_arrlen(g_textures);
	for (int i = g_textures_len - 1; i >= 0; --i) {
		shader_set_uniform_float(&g_shader, "u_parallax_offset", fmodf((engine->time_elapsed * 0.01f) * (4 - i + 1), 1.0f));
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
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
	};

	vbuffer_init(vbuf);
	vbuffer_set_data(vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(vbuf, shader, "a_position", 2, GL_FLOAT, 0, 0);
}

