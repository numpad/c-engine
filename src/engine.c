#include "engine.h"

int engine_init(engine_t *engine) {
	engine->window = SDL_CreateWindow("Soil Soldiers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 450, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	engine->window_id = SDL_GetWindowID(engine->window);
	engine->is_running = 1;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetSwapInterval(1);

	engine->gl_ctx = SDL_GL_CreateContext(engine->window);

	glClearColor(183.0f / 255.0f, 255.0f / 255.0f, 210.0f / 255.0f, 1.0f);
	glViewport(0, 0, 450, 800);

	return 0;
}

int engine_destroy(engine_t *engine) {
	SDL_DestroyWindow(engine->window);
	SDL_GL_DeleteContext(engine->gl_ctx);
	return 0;
}

void engine_update(engine_t *engine) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				engine->is_running = 0;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.windowID == engine->window_id && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					glViewport(0, 0, event.window.data1, event.window.data2);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				SDL_MouseButtonEvent *mouse_event = (SDL_MouseButtonEvent *)&event;
				engine->mouseX = mouse_event->x;
				engine->mouseY = mouse_event->y;
				glClearColor(engine->mouseX / 450.0f, engine->mouseY / 800.0f, 0.0f, 1.0f);
				break;
			}
		}
	}
}

void engine_draw(engine_t *engine) {
	glClear(GL_COLOR_BUFFER_BIT);
	
	

	SDL_GL_SwapWindow(engine->window);
}

void engine_mainloop(engine_t *engine) {
	engine_update(engine);
	engine_draw(engine);
}

void engine_mainloop_emcc(void *engine) {
	engine_mainloop((engine_t *)engine);
}

