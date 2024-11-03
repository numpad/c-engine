#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdlib.h>

struct engine_s;

enum console_msg_type {
	CONSOLE_MSG_DEFAULT,
	CONSOLE_MSG_INFO,
	CONSOLE_MSG_SUCCESS,
	CONSOLE_MSG_ERROR,
};

struct console_msg_s {
	char *message;
	double created_at;
	float duration;

	enum console_msg_type type;

	float fade_in_animation;
};

struct console_s {
	struct console_msg_s messages[16];
	size_t messages_max;
	size_t messages_count;

	char *input_text;
	size_t input_text_maxlen;
};

void console_init(struct console_s *);
void console_destroy(struct console_s *);

void console_update(struct console_s *, struct engine_s *, float dt);
void console_draw(struct console_s *, struct engine_s *);

void console_add_input_text(struct console_s *, char *text);
void console_log(struct engine_s *, char *fmt, ...);
void console_log_ex(struct engine_s *, enum console_msg_type type, float duration, char *fmt, ...);

#endif

