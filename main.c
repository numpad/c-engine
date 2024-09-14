#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "engine.h"

int main(int argc, char *argv[]) {
	struct engine_s *engine = engine_new(argc, argv);
	if (engine == NULL) {
		SDL_Log("failed initializing engine...\n");
		return 1;
	}

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(engine_mainloop_emcc, engine, 0, 1);
#else
	engine_enter_mainloop(engine);
#endif

	engine_destroy(engine);
	return 0;
}

