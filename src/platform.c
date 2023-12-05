
#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <emscripten.h>

void platform_vibrate(int ms) {
	char js[32] = {0};
	snprintf(js, 32, "navigator.vibrate(%d);", ms);
	emscripten_run_script(js);
}

#else

void platform_vibrate(int ms) {
	// not supported
	// TODO: rumble SDL gamepad if connected?
}

#endif

