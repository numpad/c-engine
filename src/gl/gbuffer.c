#include "gbuffer.h"

#include <assert.h>
#include <SDL.h>
#include "gl/opengles3.h"
#include "engine.h"
#include "gl/shader.h"
#include "gl/camera.h"

static void init_gbuffer_texture(
	struct gbuffer *gbuffer,
	enum gbuffer_texture target, GLenum attachment,
	GLint internalformat, GLenum type, int width, int height
	);

static void setup_fullscreen_triangle(struct gbuffer *);
static void draw_fullscreen_triangle(struct gbuffer, struct engine *);

////////////////
// PUBLIC API //
////////////////

void gbuffer_init(struct gbuffer *gbuffer, struct engine *engine) {
	assert(gbuffer != NULL);
	assert(engine != NULL);

	setup_fullscreen_triangle(gbuffer);
	int width = engine->window_highdpi_width;
	int height = engine->window_highdpi_height;
	assert(width > 0 && height > 0);

	// Setup textures
	glGenTextures(GBUFFER_TEXTURE_MAX, &gbuffer->textures[0]);
	glGenFramebuffers(1, &gbuffer->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gbuffer->framebuffer);
	init_gbuffer_texture(gbuffer,
			GBUFFER_TEXTURE_ALBEDO, GL_COLOR_ATTACHMENT0,
			GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	init_gbuffer_texture(gbuffer,
			GBUFFER_TEXTURE_POSITION, GL_COLOR_ATTACHMENT1,
			GL_RGBA16F, GL_FLOAT, width, height);
	init_gbuffer_texture(gbuffer,
			GBUFFER_TEXTURE_NORMAL, GL_COLOR_ATTACHMENT2,
			GL_RGBA16F, GL_FLOAT, width, height);
	// Depth-stencil attachment
	glGenRenderbuffers(1, &gbuffer->renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, gbuffer->renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gbuffer->renderbuffer);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	glDrawBuffers(3, (GLuint[]){ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Setup shader
	shader_init_from_dir(&gbuffer->shader, "res/shader/lighting_pass/");
	shader_use(&gbuffer->shader);
	shader_set_kind(&gbuffer->shader, SHADER_KIND_GBUFFER);
	shader_set_uniform_buffer(&gbuffer->shader, "Global", &engine->shader_global_ubo);

	// color lut
	struct texture_settings_s lut_settings = TEXTURE_SETTINGS_INIT;
	lut_settings.filter_min = GL_NEAREST;
	lut_settings.filter_mag = GL_NEAREST;
	lut_settings.wrap_s     = GL_CLAMP_TO_EDGE;
	lut_settings.wrap_t     = GL_CLAMP_TO_EDGE;
	lut_settings.flip_y     = 1;
	texture_init_from_image(&gbuffer->color_lut, "res/image/lut32_night.png", &lut_settings);
}

void gbuffer_destroy(struct gbuffer *gbuffer) {
	glDeleteTextures(GBUFFER_TEXTURE_MAX, &gbuffer->textures[0]);
	glDeleteRenderbuffers(1, &gbuffer->renderbuffer);
	glDeleteFramebuffers(1, &gbuffer->framebuffer);
	shader_destroy(&gbuffer->shader);
	glDeleteBuffers(1, &gbuffer->fullscreen_vbo);
	texture_destroy(&gbuffer->color_lut);
}

void gbuffer_resize(struct gbuffer *gbuffer, int new_width, int new_height) {
	for (uint i = 0; i < GBUFFER_TEXTURE_MAX; ++i) {
		GLuint texture = gbuffer->textures[i];
		GLint internalformat = gbuffer->textures_internalformats[i];
		GLenum type = gbuffer->textures_type[i];

		// TODO: Create a new texture with the new size and copy previous data to it?
		//       Just nice to have, but less artifacts during resize?
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, internalformat, new_width, new_height, 0, GL_RGBA, type, NULL);
	}

	glBindRenderbuffer(GL_RENDERBUFFER, gbuffer->renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, new_width, new_height);
}

void gbuffer_bind(struct gbuffer gbuffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, gbuffer.framebuffer);
}

void gbuffer_unbind(struct gbuffer gbuffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gbuffer_clear(struct gbuffer gbuffer) {
#ifdef DEBUG
	GLint current_fbo;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
	assert(current_fbo == (GLint)gbuffer.framebuffer);
#endif

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void gbuffer_display(struct gbuffer gbuffer, struct camera *camera, struct engine *engine) {
	assert(gbuffer.shader.kind == SHADER_KIND_GBUFFER);
	shader_use(&gbuffer.shader);
	// gbuffer inputs
	shader_set_int(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.albedo,   0);
	shader_set_int(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.position, 1);
	shader_set_int(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.normal,   2);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gbuffer.textures[GBUFFER_TEXTURE_ALBEDO]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gbuffer.textures[GBUFFER_TEXTURE_POSITION]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gbuffer.textures[GBUFFER_TEXTURE_NORMAL]);
	// color lut
	shader_set_float(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.lut_size, 32.0f);
	shader_set_int(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.color_lut, 3);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gbuffer.color_lut.texture);

	shader_set_float(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.z_near, camera->z_near);
	shader_set_float(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.z_far,  camera->z_far);
	// TODO: Remove these:
	shader_set_float(&gbuffer.shader, gbuffer.shader.uniforms.gbuffer.time, engine->time_elapsed);
	
	// Render
	draw_fullscreen_triangle(gbuffer, engine);
	// Copy gbuffer depth to default framebuffer so we can combine it
	// with forward rendering.
	// TODO: Make this optional? Sometimes it is not needed (or even not desired)
	// NOTE: This only works with MSAA disabled.
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(
			0, 0, engine->window_highdpi_width, engine->window_highdpi_height,
			0, 0, engine->window_highdpi_width, engine->window_highdpi_height,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

////////////
// STATIC //
////////////

static void init_gbuffer_texture(
	struct gbuffer *gbuffer, enum gbuffer_texture target, GLenum attachment,
	GLint internalformat, GLenum type, int width, int height)
{
	gbuffer->textures_internalformats[target] = internalformat;
	gbuffer->textures_type[target] = type;

	GLuint texture = gbuffer->textures[target];
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, GL_RGBA, type, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
}


static void setup_fullscreen_triangle(struct gbuffer *gbuffer) {
	static const float vertices[] = {
		-1.0f, -1.0f,
		 3.0f, -1.0f,
		-1.0f,  3.0f
	};

	glGenBuffers(1, &gbuffer->fullscreen_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gbuffer->fullscreen_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
}

static void draw_fullscreen_triangle(struct gbuffer gbuffer, struct engine *engine) {
	glBindBuffer(GL_ARRAY_BUFFER, gbuffer.fullscreen_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

