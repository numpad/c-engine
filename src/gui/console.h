#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdlib.h>

struct engine_s;

enum console_msg_type {
	CONSOLE_MSG_DEFAULT,
	CONSOLE_MSG_SUCCESS
};

struct console_msg_s {
	char *message;
	double created_at;

	enum console_msg_type type;
};

struct console_s {
	struct console_msg_s messages[64];
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

#endif

