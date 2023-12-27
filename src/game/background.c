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

static void init_vbuffer_rect(vbuffer_t *vbuf, int shader);

//
// vars
//

static int g_shader = 0;
static struct texture_s g_texture = {0};
static struct vbuffer_s g_vbuffer = {0};

//
// public api
//

void background_set_image(const char *filename) {
	// load image
	texture_init_from_image(&g_texture, filename, NULL);
	
	// load shader
	g_shader = shader_from_dir("res/shader/background/");
	assert(g_shader != 0);

	float mvp[16] = GLM_MAT4_IDENTITY_INIT;
	shader_set_uniform_mat4(g_shader, "u_mvp", mvp);
	
	// load vertices
	init_vbuffer_rect(&g_vbuffer, g_shader);
}

void background_destroy(void) {
	assert(g_shader != 0);

	shader_delete(g_shader);
	g_shader = 0;
	vbuffer_destroy(&g_vbuffer);
	texture_destroy(&g_texture);
}

void background_draw(void) {
	if (g_shader == 0) return;

	glUseProgram(g_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_texture.texture);
	vbuffer_draw(&g_vbuffer, 6);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}


//
// private implementations
//

static void init_vbuffer_rect(vbuffer_t *vbuf, int shader) {
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

