#include "battlebadgers.h"

#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <stb_ds.h>
#include "engine.h"
#include "gl/shader.h"

static void battlebadgers_load(struct battlebadgers_s *scene, struct engine_s *engine) {
	scene->balls_len = 0;
	scene->balls = NULL;

	// prepare background vertices
	scene->bg_shader = shader_new("res/shader/background/vertex.glsl", "res/shader/background/fragment.glsl");
	GLfloat vertices[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
	};

	glUseProgram(scene->bg_shader);

	glGenBuffers(1, &scene->bg_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, scene->bg_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	GLint a_position = glGetAttribLocation(scene->bg_shader, "a_position");
	glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(a_position);

	GLfloat mvp[] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	GLint u_mvp = glGetUniformLocation(scene->bg_shader, "u_mvp");
	glUniform4fv(u_mvp, 4, mvp);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

}

static void battlebadgers_destroy(struct battlebadgers_s *scene, struct engine_s *engine) {
	shader_delete(scene->bg_shader);
}

static void battlebadgers_update(struct battlebadgers_s *scene, struct engine_s *engine, float dt) {
	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	if (mstate & SDL_BUTTON(1)) {
		scene->balls_len++;

		pos_s ball;
		ball.r = 20.0f + 10.0f * ((rand() % 10000) / 10000.0f);
		ball.x = mx;
		ball.y = my;
		ball.vx = 0.0f;
		ball.vy = 0.0f;

		stbds_arrput(scene->balls, ball);
	}

	for (unsigned int i = 0; i < scene->balls_len; ++i) {
		pos_s *ball = &scene->balls[i];

		ball->vy += 0.5f;
		ball->y += ball->vy;

		if (ball->y > engine->window_height - ball->r) {
			ball->y = engine->window_height - ball->r;
			ball->vy *= -1.0f;
		}
	}
}

static void battlebadgers_draw(struct battlebadgers_s *scene, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;
	
	for (unsigned int i = 0; i < scene->balls_len; ++i) {
		nvgBeginPath(vg);
		nvgCircle(vg, scene->balls[i].x, scene->balls[i].y, scene->balls[i].r);
		nvgFillColor(vg, nvgLerpRGBA(nvgRGBf(1.0f, 0.4f, 0.2f), nvgRGBf(0.8f, 0.51f, 0.32f), 0.5f));
		nvgFill(vg);
	}

	glUseProgram(scene->bg_shader);
	glBindBuffer(GL_ARRAY_BUFFER, scene->bg_vbo);
	glEnableVertexAttribArray(glGetAttribLocation(scene->bg_shader, "a_position"));
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glUseProgram(0);
}


void battlebadgers_init(struct battlebadgers_s *battlebadgers, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)battlebadgers, engine);

	// init function pointers
	battlebadgers->base.load = (scene_load_fn)battlebadgers_load;
	battlebadgers->base.destroy = (scene_destroy_fn)battlebadgers_destroy;
	battlebadgers->base.update = (scene_update_fn)battlebadgers_update;
	battlebadgers->base.draw = (scene_draw_fn)battlebadgers_draw;

}

