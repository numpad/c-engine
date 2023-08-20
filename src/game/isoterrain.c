#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"

static void index_to_pos(struct isoterrain_s *terrain, size_t index, int *x, int *y, int *z) {
	*z = index / (terrain->width * terrain->height);
	index -= ((*z) * terrain->width * terrain->height);
	*y = index / terrain->width;
	*x = index % terrain->height;
}

static void pos_to_index(struct isoterrain_s *terrain, int x, int y, int z, int *index) {
	*index = x + (y * terrain->width) + (z * terrain->width * terrain->height);
}

static iso_block isoterrain_get_block(struct isoterrain_s *terrain, int x, int y, int z) {
	if (x < 0 || y < 0 || z < 0 || x >= terrain->width || y >= terrain->height || z >= terrain->depth) return -1;

	const int pos = x + y * terrain->width;
	return terrain->blocks[pos];
}

void isoterrain_init(struct isoterrain_s *terrain, int w, int h, int depth) {
	terrain->width = w;
	terrain->height = h;
	terrain->depth = depth;
	terrain->blocks = malloc(w * h * depth * sizeof(iso_block));

	terrain->shader = shader_new("res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
	terrain->texture = texture_from_image("res/image/iso_tileset.png", NULL);

	terrain->vbuf = malloc(sizeof(struct vbuffer_s));
	GLfloat vertices[] = {
		-0.25f, -0.25f,  0.0f, 0.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		-0.25f,  0.25f,  0.0f, 16.0f/17.0f,
		-0.25f,  0.25f,  0.0f, 16.0f/17.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		 0.25f,  0.25f,  1.0f, 16.0f/17.0f,
	};
	vbuffer_init(terrain->vbuf);
	vbuffer_set_data(terrain->vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glUseProgram(terrain->shader);
	glUniform1i(glGetUniformLocation(terrain->shader, "u_texture"), 0);

	// TODO: set some blocks for testing
	for (int i = 0; i < terrain->width * terrain->height * terrain->depth; ++i) {
		terrain->blocks[i] = -1;
	}
	for (int x = 0; x < terrain->width; ++x) {
		for (int y = 0; y < terrain->height; ++y) {
			isoterrain_set_block(terrain, x, y, 0, 9 * 16);
		}
	}

	for (int x = 0; x < terrain->width; ++x) {
		isoterrain_set_block(terrain, x, 0, 1, 129);
		
		if (x < 3) isoterrain_set_block(terrain, x, 1, 1, 145);
		else isoterrain_set_block(terrain, x, 1, 1, 129);

		if (x < 4) isoterrain_set_block(terrain, x, 2, 1, 145);
		else isoterrain_set_block(terrain, x, 2, 1, 129);

		for (int y = 3; y < terrain->height; ++y) {
			isoterrain_set_block(terrain, x, y, 1, 145);
		}

		if (x > 2 && x != terrain->width - 2) {
			isoterrain_set_block(terrain, x, 4, 1, 80);
		}
	}

	for (int y = 0; y < terrain->height; ++y) {
		if (y < 3) isoterrain_set_block(terrain, terrain->width - 2, y, 1, 132);
		else isoterrain_set_block(terrain, terrain->width - 2, y, 1, 148);
		if (y == 4) isoterrain_set_block(terrain, terrain->width - 2, y, 1, 24);

		if (y < 3) isoterrain_set_block(terrain, terrain->width - 5, y, 1, 65);
		else if (y == 3) isoterrain_set_block(terrain, terrain->width - 5, y, 1, 81);
		else if (y == 4) isoterrain_set_block(terrain, terrain->width - 5, y, 1, 86);
	}


	isoterrain_set_block(terrain, terrain->width - 1, 0, 2, 8 + 7 * 16);
	isoterrain_set_block(terrain, terrain->width - 4, 0, 2, 8 + 8 * 16);
	isoterrain_set_block(terrain, terrain->width - 4, terrain->height - 1, 2, 8 + 7 * 16);
	isoterrain_set_block(terrain, terrain->width - 6, terrain->height - 1, 2, 0 + 1 * 16);
	isoterrain_set_block(terrain, terrain->width - 5, terrain->height - 1, 2, 0 + 1 * 16);
	isoterrain_set_block(terrain, terrain->width - 7, terrain->height - 1, 2, 10 + 1 * 16);
	isoterrain_set_block(terrain, terrain->width - 8, terrain->height - 1, 2, 10 + 1 * 16);
	isoterrain_set_block(terrain, 1, terrain->height - 1, 2, 0 + 1 * 16);
	isoterrain_set_block(terrain, 0, terrain->height - 1, 2, 0 + 0 * 16);
	isoterrain_set_block(terrain, terrain->width - 1, terrain->height - 1, 2, 8 + 6 * 16);
	isoterrain_set_block(terrain, 0, terrain->height - 3, 2, 8 + 6 * 16);
	isoterrain_set_block(terrain, 3, 2, 2, 8 + 6 * 16);
	isoterrain_set_block(terrain, 0, 0, 2, 8 + 6 * 16);
	isoterrain_set_block(terrain, 0, 3, 2, 8 + 8 * 16);

	isoterrain_set_block(terrain, 2, terrain->height - 1, 1, 0 + 1 * 16);
	isoterrain_set_block(terrain, 3, terrain->height - 1, 1, 0 + 1 * 16);
}

void isoterrain_init_from_file(struct isoterrain_s *terrain, const char *path_to_script) {
	/*
	lua_State *L = luaL_newstate();
	if (L == NULL) {
		fprintf(stderr, "could not create lua context!\n");
		return;
	}

	if (luaL_dofile(L, path_to_script) != 0) {
		fprintf(stderr, "failed executing script '%s': %s\n", path_to_script, lua_tostring(L, -1));
		lua_close(L);
		return;
	}
	
	// get width, height and depth
	lua_getglobal(L, "width");
	const int width = lua_tointeger(L, -1);
	lua_pop(L, -1);
	lua_getglobal(L, "height");
	const int height = lua_tointeger(L, -1);
	lua_pop(L, -1);
	lua_getglobal(L, "depth");
	const int depth = lua_tointeger(L, -1);
	lua_pop(L, -1);

	// initialize terrain
	isoterrain_init(terrain, width, height, depth);

	// load blocks
	lua_getglobal(L, "blocks");
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		// key is at -2, value at -1.
		if (lua_isnumber(L, -2)) {
			const int key = lua_tointeger(L, -2);

			if (key > 0) {
				terrain->blocks[key - 1] = lua_tointeger(L, -1);
			}
		}

		lua_pop(L, 1); // remove value, keep key for next iteration.
	}

	lua_close(L);
	*/
}

void isoterrain_destroy(struct isoterrain_s *terrain) {
	free(terrain->blocks);
	vbuffer_destroy(terrain->vbuf);
	free(terrain->vbuf);
}

void isoterrain_draw(struct isoterrain_s *terrain, const mat4 proj, const mat4 view) {
	glUseProgram(terrain->shader);

	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_projection"), 1, GL_FALSE, proj[0]);
	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_view"), 1, GL_FALSE, view[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain->texture);
	// draw left to right, top to bottom
	for (int z = 0; z < terrain->depth; ++z) {
		for (int x = 0; x < terrain->width; ++x) {
			for (int y = terrain->height - 1; y >= 0; --y) {
				int index;
				pos_to_index(terrain, x, y, z, &index);

				iso_block *block = &terrain->blocks[index];
				if (*block == -1) continue;

				const float tx = *block % 16;
				const float ty = floor(*block / 15.0f + (1.0f / 256.0f));
				const float bx = (float)(x - z) - terrain->width * 0.9f;
				const float by = (float)y + z;
				const float bz = 0.0f;

				// TODO: remove
				glUniform2f(glGetUniformLocation(terrain->shader, "u_tilesize"), (1.0f / 256.0f) * 16.0f, (1.0f / 256.0f) * 17.0f);
			
				glUniform2f(glGetUniformLocation(terrain->shader, "u_tilepos"), tx, ty);
				glUniform3f(glGetUniformLocation(terrain->shader, "u_pos"), bx, by, bz);
				vbuffer_draw(terrain->vbuf, 6);
			}
		}
	}
}

void isoterrain_set_block(struct isoterrain_s *terrain, int x, int y, int z, iso_block block) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height || z < 0 || z >= terrain->depth) return;

	int index;
	pos_to_index(terrain, x, y, z, &index);
	terrain->blocks[index] = block;
}

