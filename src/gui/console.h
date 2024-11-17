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

enum console_msg_state {
	CONSOLE_MSG_STATE_IN = 0,
	CONSOLE_MSG_STATE_VISIBLE = 1,
	CONSOLE_MSG_STATE_OUT = 2,
	_CONSOLE_MSG_STATE_MAX
};

struct console_msg_s {
	char *message;
	double created_at;
	float duration[_CONSOLE_MSG_STATE_MAX];

	float animation;
	enum console_msg_type type;
	enum console_msg_state state;
};

struct console_s {
	struct console_msg_s messages[16];
	size_t messages_max;
	size_t messages_count;
};

void console_init(struct console_s *);
void console_destroy(struct console_s *);

void console_update(struct console_s *, struct engine_s *, float dt);
void console_draw(struct console_s *, struct engine_s *);

void console_log(struct engine_s *, const char *fmt, ...);
void console_log_ex(struct engine_s *, enum console_msg_type type, float duration, const char *fmt, ...);

#endif

