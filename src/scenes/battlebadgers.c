#include "battlebadgers.h"

#include <math.h>
#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include "engine.h"
#include "gl/shader.h"

static void battlebadgers_load(struct battlebadgers_s *scene, struct engine_s *engine) {
	scene->balls_len = 0;
	scene->balls = NULL;

	int tw, th, tn;
	unsigned char *tpixels = stbi_load("res/image/space_bg.png", &tw, &th, &tn, 3);
	if (tpixels != NULL) {
		glGenTextures(1, &scene->bg_texture);
		glBindTexture(GL_TEXTURE_2D, scene->bg_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tw, th, 0, GL_RGB, GL_UNSIGNED_BYTE, tpixels);
		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(tpixels);
	}


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



	for (int i = 0; i < 10; ++i) {
		scene->balls_len++;

		pos_s ball;
		ball.r = 15.0f;
		ball.x = 200.0f + 30.0f * i;
		ball.y = 400.0f;
		ball.vx = 0.0f;
		ball.vy = 0.0f;
		ball.fixed = 1;

		stbds_arrput(scene->balls, ball);
	}
}

static void battlebadgers_destroy(struct battlebadgers_s *scene, struct engine_s *engine) {
	shader_delete(scene->bg_shader);
}

static void battlebadgers_update(struct battlebadgers_s *scene, struct engine_s *engine, float dt) {
	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	if (mstate & SDL_BUTTON(3) || mstate & SDL_BUTTON(1)) {
		scene->balls_len++;

		pos_s ball;
		ball.r = 15.0f;
		ball.x = mx;
		ball.y = my;
		ball.vx = 0.0f;
		ball.vy = 0.0f;
		ball.fixed = (mstate & SDL_BUTTON(1)) ? 1 : 0;

		stbds_arrput(scene->balls, ball);
	}

	for (unsigned int i = 0; i < scene->balls_len; ++i) {
		pos_s *ball = &scene->balls[i];

		if (!ball->fixed) {
			ball->vy += 0.35f;
			ball->x += ball->vx;
			ball->y += ball->vy;


			for (unsigned int j = 0; j < scene->balls_len; ++j) {
				pos_s *fixed_ball = &scene->balls[j];
				if (!fixed_ball->fixed) continue;

				const float dx = fabs(fixed_ball->x - ball->x);
				const float dy = fabs(fixed_ball->y - ball->y);
				const float d2 = (dx*dx + dy*dy);
				const float r2 = (ball->r + fixed_ball->r) * (ball->r + fixed_ball->r);
				if (d2 <= r2) {
					ball->vy *= -1.0f;
				}
			}
		}
	}
}

static void battlebadgers_draw(struct battlebadgers_s *scene, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;
	
	for (unsigned int i = 0; i < scene->balls_len; ++i) {
		nvgBeginPath(vg);
		nvgCircle(vg, scene->balls[i].x, scene->balls[i].y, scene->balls[i].r);
		nvgFillPaint(vg, nvgRadialGradient(vg, scene->balls[i].x - 5.0f, scene->balls[i].y - 5.0f, 2.0f, 15.0f,
					nvgRGBf(1.0f, 1.0f, 1.0f), nvgRGBf(0.2f, 0.22f, 0.7f)));
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgCircle(vg, scene->balls[i].x, scene->balls[i].y, scene->balls[i].r);
		nvgStrokeColor(vg, nvgRGBf(0.2f, 0.2f, 0.2f));
		nvgStrokeWidth(vg, 3.0f);
		nvgStroke(vg);
	}

	glUseProgram(scene->bg_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, scene->bg_texture);
	glBindBuffer(GL_ARRAY_BUFFER, scene->bg_vbo);
	glEnableVertexAttribArray(glGetAttribLocation(scene->bg_shader, "a_position"));
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindTexture(GL_TEXTURE_2D, 0);
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

