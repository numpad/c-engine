#include "console.h"

#include <string.h>
#include <stdarg.h>
#include <stb_ds.h>
#include <nanovg.h>
#include "engine.h"

// deep copy the message so that console can manage message lifetimes
static void console_msg_copy(struct console_msg_s *from, struct console_msg_s *to) {
	const size_t msg_len = strlen(from->message);
	char *newmsg = malloc(msg_len * sizeof(char) + 1);
	strcpy(newmsg, from->message);
	to->message = newmsg;
}

static size_t console_new_messages_count(struct console_s *console) {
	const int visible = stbds_arrlen(console->messages) - console->new_messages_from_index;

	return visible > 0 ? visible : 0;
}

void console_init(struct console_s *console) {
	console->messages = NULL;
	console->new_messages_from_index = 0;
	console->input_text_maxlen = 1024;
	console->input_text = malloc(console->input_text_maxlen * sizeof(char));
	console->last_message_added_at = -1.0f;
}

void console_destroy(struct console_s *console) {
	const size_t messages_len = stbds_arrlen(console->messages);
	for (size_t i = 0; i < messages_len; ++i) {
		free(console->messages[i].message);
	}

	stbds_arrfree(console->messages);
	free(console->input_text);
	console->input_text_maxlen = 0;
}

void console_update(struct console_s *console, struct engine_s *engine, float dt) {
	const float msg_visibility_duration = 4.0f; // 4 seconds
	
	static float t = 0.0f;
	t += dt;

	if (console_new_messages_count(console) > 0) {
		if (t >= msg_visibility_duration) {
			t -= msg_visibility_duration;
			console->new_messages_from_index++;
		}
	} else {
		t = 0.0f;
	}
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
	for (int i = console->new_messages_from_index; i < msgs_len; ++i) {
		const int local_i = i - console->new_messages_from_index;
		float bounds[4];
		nvgTextBounds(vg, 0.0f, 0.0f, console->messages[i].message, NULL, bounds);

		const float y_offset = (local_i + 1.0f) * (bounds[3] - bounds[1]);
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
	console_msg_copy(&msg, &msg);
	stbds_arrput(console->messages, msg);
}

void console_log(struct console_s *console, char *fmt, ...) {
	va_list argp;

	va_start(argp, fmt);
	
	int len = snprintf(NULL, 0, fmt, argp);
	char buf[len + 1];
	buf[len] = '\0';

	va_end(argp);

	va_start(argp, fmt);
	snprintf(buf, len, fmt, argp);

	console_add_message(console, (struct console_msg_s){ .message = buf });

	va_end(argp);

}
