#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "src/engine.h"

int main(int argc, char *argv[]) {
	engine_t engine;
	if (engine_init(&engine)) {
		fprintf(stderr, "failed initializing engine...\n");
		return 1;
	}

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(engine_mainloop_emcc, &engine, 0, 1);
#else
	while (engine.is_running) {
		engine_mainloop(&engine);
	}
#endif

	engine_destroy(&engine);
	return 0;
}

