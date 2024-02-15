#include "cards.h"

#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"

void cardrenderer_init(struct cardrenderer_s *renderer, const char *tileset) {
	struct texture_settings_s tconf = TEXTURE_SETTINGS_INIT;
	tconf.filter_min = GL_LINEAR_MIPMAP_LINEAR;
	tconf.filter_mag = GL_NEAREST;
	tconf.wrap_s = GL_CLAMP_TO_EDGE;
	tconf.wrap_t = GL_CLAMP_TO_EDGE;
	tconf.gen_mipmap = 1;
	tconf.flip_y = 1;

	texture_init_from_image(&renderer->texture_atlas, tileset, &tconf);
	shader_init_from_dir(&renderer->shader, "res/shader/card");
	vbuffer_init(&renderer->vbo);

	GLfloat vertices[] = {
		-0.5f, -1.0f,  0.0f, 0.0f,
		 0.5f, -1.0f,  1.0f, 0.0f,
		-0.5f,  1.0f,  0.0f, 2.0f,
		-0.5f,  1.0f,  0.0f, 2.0f,
		 0.5f, -1.0f,  1.0f, 0.0f,
		 0.5f,  1.0f,  1.0f, 2.0f,
	};
	vbuffer_set_data(&renderer->vbo, sizeof(vertices), vertices);
	vbuffer_set_attrib(&renderer->vbo, &renderer->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(&renderer->vbo, &renderer->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void cardrenderer_destroy(struct cardrenderer_s *renderer) {
	texture_destroy(&renderer->texture_atlas);
	shader_destroy(&renderer->shader);
	vbuffer_destroy(&renderer->vbo);
}

