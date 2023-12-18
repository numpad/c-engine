
//
// vibration
//

const int PLATFORM_VIBRATE_TAP   = 30;
const int PLATFORM_VIBRATE_SHORT = 75;

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

//
// open links in webbrowser
//

#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <emscripten.h>

void platform_open_website(const char *url) {
	char js[512] = {0};
	snprintf(js, 512, "window.open('%s').focus();", url);
	emscripten_run_script(js);
}

#elif __linux__

#include <stdio.h>
#include <stdlib.h>

void platform_open_website(const char *url) {
	char cmd[512] = {0};
	snprintf(cmd, 512, "xdg-open \"%s\" &", url);

	// TODO: this is blocking, so we use an "&" in the call. maybe fork() instead?
	system(cmd);
}

#elif _WIN32

// TODO: completely untested code:
#include <windows.h>

void platform_open_website(const char *url) {
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

#elif __APPLE__

#include <stdio.h>
#include <stdlib.h>

// TODO: completely untested code:
void platform_open_website(const char *url) {
	char cmd[512] = {0};
	snprintf(cmd, 512, "open \"%s\" &", url);

	// TODO: this is blocking, so we use an "&" in the call. maybe fork() instead?
	system(cmd);
}

#else
	#error "Platform not supported!"
#endif

