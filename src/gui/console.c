#include "console.h"

#include <string.h>
#include <stdarg.h>
#include <nanovg.h>
#include "engine.h"


static void console_add_message(struct console_s *, struct console_msg_s msg);
static void console_remove_message(struct console_s *, int message_i);

// deep copy the message so that console can manage message lifetimes
static void console_msg_copy(struct console_msg_s *from, struct console_msg_s *to);

static void get_color_for_msg(struct console_msg_s *, NVGcolor *bg, NVGcolor *fg, NVGcolor *outline, NVGcolor *muted);

////////////
// PUBLIC //
////////////

void console_init(struct console_s *console) {
	console->messages_max = 16;
	console->messages_count = 0;
}

void console_destroy(struct console_s *console) {
	for (size_t i = 0; i < console->messages_count; ++i) {
		free(console->messages[i].message);
	}
}

void console_update(struct console_s *console, struct engine_s *engine, float dt) {
	for (size_t i = 0; i < console->messages_count; ++i) {
		struct console_msg_s *msg = &console->messages[i];
		const float state_duration = msg->duration[msg->state];
		
		switch (msg->state) {
		case CONSOLE_MSG_STATE_IN:
			msg->animation = glm_clamp(
				(engine->time_elapsed - msg->created_at) / state_duration,
				0.0f, 1.0f);
			if (msg->animation >= 1.0f) {
				msg->animation = 0.0f;
				msg->state = CONSOLE_MSG_STATE_VISIBLE;
			}
			break;
		case CONSOLE_MSG_STATE_VISIBLE:
			msg->animation = glm_clamp(
				(engine->time_elapsed - (msg->created_at + msg->duration[CONSOLE_MSG_STATE_IN])) / state_duration,
				0.0f, 1.0f);
			if (msg->animation >= 1.0f) {
				msg->animation = 0.0f;
				msg->state = CONSOLE_MSG_STATE_OUT;
			}
			break;
		case CONSOLE_MSG_STATE_OUT:
			msg->animation = glm_clamp(
				(engine->time_elapsed - (msg->created_at + msg->duration[CONSOLE_MSG_STATE_IN] + msg->duration[CONSOLE_MSG_STATE_VISIBLE])) / state_duration,
				0.0f, 1.0f);
			if (msg->animation >= 1.0f) {
				console_remove_message(console, i);
				--i;
				continue;
			}
			break;
		case _CONSOLE_MSG_STATE_MAX:
		default:
			break;
		};
	}
}

void console_draw(struct console_s *console, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;

	const float msg_h = 30.0f;
	const float con_x = engine->window_width - 23.0f;
	const float con_y = 60.0f;

	nvgFontFaceId(vg, engine->font_default_bold);
	nvgFontSize(vg, 12.0f);
	nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
	for (size_t i = 0; i < console->messages_count; ++i) {
		struct console_msg_s *msg = &console->messages[i];
		float text_w;
		{
			float bounds[4];
			nvgTextBounds(vg, 0.0f, 0.0f, msg->message, NULL, bounds);
			text_w = bounds[2] - bounds[0];
		}

		const float y_offset = (i + 1.0f) * (msg_h + 9.0f);
		const float msg_x = con_x + (
				(msg->state == CONSOLE_MSG_STATE_IN)  ? 60.0f * glm_ease_elast_in(1.0f - msg->animation) :
				(msg->state == CONSOLE_MSG_STATE_OUT) ? (text_w + msg_h) * glm_ease_back_in(msg->animation):
				0.0f);

		const float msg_y = con_y + y_offset;

		NVGcolor color_bg, color_fg, color_outline, color_muted;
		get_color_for_msg(msg, &color_bg, &color_fg, &color_outline, &color_muted);

		// Draw background
		nvgBeginPath(vg);
		nvgRect(vg, msg_x - text_w - msg_h * 0.5f, msg_y - msg_h * 0.5f, text_w + msg_h * 0.5f, msg_h);
		nvgCircle(vg, msg_x - text_w - msg_h * 0.5f, msg_y, msg_h * 0.5f);
		nvgCircle(vg, msg_x - text_w + text_w, msg_y, msg_h * 0.5f);
		nvgStrokeWidth(vg, 4.0f);
		nvgStrokeColor(vg, color_outline);
		nvgStroke(vg);
		nvgFillColor(vg, color_bg);
		nvgFill(vg);


		const float time_remaining = glm_clamp(1.0f - (engine->time_elapsed - msg->created_at) / ((msg->duration[0] + msg->duration[1] + msg->duration[2])), 0.0f, 1.0f);
		const float angle_offset = glm_rad(-90.0f);
		nvgBeginPath(vg);
		nvgArc(vg, msg_x - text_w - msg_h * 0.5f, msg_y, msg_h * 0.5f - 4.0f, angle_offset, angle_offset + glm_rad(360.0f * glm_ease_quad_in(time_remaining)), NVG_CW);
		nvgStrokeWidth(vg, 4.0f);
		nvgStrokeColor(vg, color_muted);
		nvgStroke(vg);

		// Draw text
		nvgFontBlur(vg, 0.0f);
		nvgFillColor(vg, color_fg);
		nvgText(vg, msg_x, msg_y + 1.0f, msg->message, NULL);

		nvgFillColor(vg, nvgRGBAf(color_fg.r, color_fg.g, color_fg.b, color_fg.a * 0.5f));
		nvgText(vg, msg_x, msg_y + 1.0f, msg->message, NULL);
	}

}

void console_log(struct engine_s *engine, const char *fmt, ...) {
	va_list argp;

	va_start(argp, fmt);
	int len = vsnprintf(NULL, 0, fmt, argp);
	char buf[len + 1];
	va_end(argp);

	va_start(argp, fmt);
	vsnprintf(buf, len + 1, fmt, argp);
	va_end(argp);

	console_add_message(engine->console, (struct console_msg_s){
		.message = buf,
		.created_at = engine->time_elapsed,
		.duration = {1.0f, 4.0f, 0.4f},
		.animation = 0.0f,
		.type = CONSOLE_MSG_DEFAULT,
		.state = CONSOLE_MSG_STATE_IN
	});
}

void console_log_ex(struct engine_s *engine, enum console_msg_type type, float duration, const char *fmt, ...) {
	va_list argp;

	va_start(argp, fmt);
	int len = vsnprintf(NULL, 0, fmt, argp);
	char buf[len + 1];
	va_end(argp);

	va_start(argp, fmt);
	vsnprintf(buf, len + 1, fmt, argp);
	va_end(argp);

	console_add_message(engine->console, (struct console_msg_s){
		.message = buf,
		.created_at = engine->time_elapsed,
		.duration = {1.0f, duration, 0.4f},
		.animation = 0.0f,
		.type = type,
		.state = CONSOLE_MSG_STATE_IN
	});
}


////////////
// STATIC //
////////////

static void console_add_message(struct console_s *console, struct console_msg_s msg) {
	while (console->messages_count >= console->messages_max) {
		console_remove_message(console, 0);
	}

	console_msg_copy(&msg, &console->messages[console->messages_count]);
	++console->messages_count;
}

static void console_remove_message(struct console_s *console, int message_i) {
	free(console->messages[message_i].message);
	for (size_t i = message_i; i < console->messages_count; ++i) {
		console->messages[i] = console->messages[i + 1];
	}
	--console->messages_count;
}

static void console_msg_copy(struct console_msg_s *from, struct console_msg_s *to) {
	memcpy(to, from, sizeof(struct console_msg_s));
	const size_t msg_len = strlen(from->message);
	char *newmsg = malloc((msg_len + 1) * sizeof(char));
	strcpy(newmsg, from->message);
	to->message = newmsg;
}

static void get_color_for_msg(struct console_msg_s *msg, NVGcolor *bg, NVGcolor *fg, NVGcolor *outline, NVGcolor *muted) {
	switch (msg->type) {
	case CONSOLE_MSG_DEFAULT:
		*bg      = nvgRGBf (1.0f, 1.0f, 1.0f);
		*fg      = nvgRGBf (0.3f, 0.3f, 0.3f);
		*outline = nvgRGBAf(1.0f, 1.0f, 1.0f, 0.7f);
		*muted   = nvgRGBf (0.9f, 0.9f, 0.9f);
		break;
	case CONSOLE_MSG_INFO:
		*bg      = nvgRGBf (0.32f, 0.4f, 0.84f);
		*fg      = nvgRGBf (1.0f, 1.0f, 1.0f);
		*outline = nvgRGBAf(0.5f, 0.55f, 0.9f, 0.7f);
		*muted   = nvgRGBf (0.22f, 0.3f, 0.74f);
		break;
	case CONSOLE_MSG_SUCCESS:
		*bg      = nvgRGBf (0.4f, 0.84f, 0.32f);
		*fg      = nvgRGBf (1.0f, 1.0f, 1.0f);
		*outline = nvgRGBAf(0.55f, 0.9f, 0.5f, 0.7f);
		*muted   = nvgRGBf (0.3f, 0.74f, 0.22f);
		break;
	case CONSOLE_MSG_ERROR:
		*bg      = nvgRGBf (0.84f, 0.4f, 0.32f);
		*fg      = nvgRGBf (1.0f, 1.0f, 1.0f);
		*outline = nvgRGBAf(0.9f, 0.55f, 0.5f, 0.7f);
		*muted   = nvgRGBf (0.74f, 0.3f, 0.22f);
		break;
	};
}
