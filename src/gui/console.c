#include "console.h"

#include <stb_ds.h>
#include <nanovg.h>
#include "engine.h"

void console_init(struct console_s *console) {
	console->messages = NULL;
	console->input_text_maxlen = 1024;
	console->input_text = malloc(console->input_text_maxlen * sizeof(char));

	// TODO: remove. only for testing
	console_add_message(console, (struct console_msg_s){ "Round starting." });
	console_add_message(console, (struct console_msg_s){ "You sight 3 enemies, but hear more in the distance." });
}

void console_destroy(struct console_s *console) {
	stbds_arrfree(console->messages);
	free(console->input_text);
	console->input_text_maxlen = 0;
}

void console_draw(struct console_s *console, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;

	const float width = 210.0f;
	const float height = 180.0f;
	const float input_height = 20.0f;
	const float x = 5.0f;
	const float y = engine->window_height - height - 220.0f;

	const float input_y = y + height - input_height;

	// chat messages
	nvgBeginPath(vg);
	nvgRect(vg, x, y, width, height - input_height);
	nvgFillColor(vg, nvgRGBAf(0.1f, 0.1f, 0.1f, 0.4f));
	nvgFill(vg);

	// chat input
	nvgBeginPath(vg);
	nvgRect(vg, x, input_y, width, input_height);
	nvgFillColor(vg, nvgRGBAf(0.2f, 0.2f, 0.2f, 0.4f));
	nvgFill(vg);

	nvgFontSize(vg, 12.0f);
	const int msgs_len = stbds_arrlen(console->messages);
	for (int i = 0; i < msgs_len; ++i) {

		float bounds[4];
		nvgTextBounds(vg, 0.0f, 0.0f, console->messages[i].message, NULL, bounds);

		const float y_offset = (i + 1.0f) * (bounds[3] - bounds[1]);
		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgText(vg, x, y + y_offset, console->messages[i].message, NULL);
	}

	
}

void console_add_input_text(struct console_s *console, char *text) {
	// TODO: check length, realloc when needed.
	strcat(console->input_text, text);
}

void console_add_message(struct console_s *console, struct console_msg_s msg) {
	stbds_arrput(console->messages, msg);
}

