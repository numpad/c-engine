#include "gl/canvas.h"

#include <SDL_opengles2.h>
#include <assert.h>
#include "gl/texture.h"
#include "util/util.h"

void canvas_init(canvas_t *cv, int width, int height, canvas_config_t *config) {
	assert(cv != NULL);
	assert(width > 0 && height > 0);
	cv->pre_bind_viewport_width = 0;
	cv->pre_bind_viewport_height = 0;
	cv->rbo_depth = 0;
	cv->rbo_stencil = 0;
	cv->rbo_depth_stencil = 0;

	// generate texture
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	texture_init(&cv->texture, width, height, &settings);

	// generate framebuffer
	glGenFramebuffers(1, &cv->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, cv->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cv->texture.texture, 0);

	// Attachments
	if (config && config->has_stencil && config->has_depth) {
		// generate combined depth-stencil
		glGenRenderbuffers(1, &cv->rbo_depth_stencil);
		glBindRenderbuffer(GL_RENDERBUFFER, cv->rbo_depth_stencil);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cv->rbo_stencil);
	} else if (config && config->has_stencil && !config->has_depth) {
		// generate only stencil
		glGenRenderbuffers(1, &cv->rbo_stencil);
		glBindRenderbuffer(GL_RENDERBUFFER, cv->rbo_stencil);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cv->rbo_stencil);
	} else if (config && !config->has_stencil && config->has_depth) {
		// generate only depth
		glGenRenderbuffers(1, &cv->rbo_depth);
		glBindRenderbuffer(GL_RENDERBUFFER, cv->rbo_depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cv->rbo_depth);
	}

	GL_CHECK_FRAMEBUFFER();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void canvas_destroy(canvas_t *cv) {
	texture_destroy(&cv->texture);
	glDeleteFramebuffers(1, &cv->fbo);
	cv->fbo = 0;
}

void canvas_bind(canvas_t *cv) {
	assert(cv->pre_bind_viewport_width == 0);
	assert(cv->pre_bind_viewport_height == 0);

	int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	cv->pre_bind_viewport_width = vp[2];
	cv->pre_bind_viewport_height = vp[3];

	glBindFramebuffer(GL_FRAMEBUFFER, cv->fbo);
	glViewport(0, 0, cv->texture.width, cv->texture.height);
}

void canvas_unbind(canvas_t *cv) {
	assert(cv->pre_bind_viewport_width != 0);
	assert(cv->pre_bind_viewport_height != 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, cv->pre_bind_viewport_width, cv->pre_bind_viewport_height);
	cv->pre_bind_viewport_width = 0;
	cv->pre_bind_viewport_height = 0;
}

