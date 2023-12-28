#include "background.h"

#include <assert.h>
#include <cglm/cglm.h>
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
static texture_t g_texture = {0};
static vbuffer_t g_vbuffer = {0};

//
// public api
//

void background_set_image(const char *filename) {
	// load image
	texture_init_from_image(&g_texture, filename, NULL);
	
	// load shader
	shader_init_from_dir(&g_shader, "res/shader/background/");
	assert(g_shader.program != 0);

	mat4 mvp = GLM_MAT4_IDENTITY_INIT;
	shader_set_uniform_mat4(&g_shader, "u_mvp", (float *)mvp);
	
	// load vertices
	init_vbuffer_rect(&g_vbuffer, &g_shader);
}

void background_destroy(void) {
	assert(g_shader.program != 0);

	shader_destroy(&g_shader);
	g_shader.program = 0;
	vbuffer_destroy(&g_vbuffer);
	texture_destroy(&g_texture);
}

void background_draw(void) {
	if (g_shader.program == 0) return;

	glUseProgram(g_shader.program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_texture.texture);
	vbuffer_draw(&g_vbuffer, 6);
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

