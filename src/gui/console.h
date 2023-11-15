#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdlib.h>

struct engine_s;

struct console_msg_s {
	char *message;
	// sender, timestamp
};

struct console_s {
	struct console_msg_s *messages;
	size_t new_messages_from_index;
	double last_message_added_at;

	char *input_text;
	size_t input_text_maxlen;
};

void console_init(struct console_s *);
void console_destroy(struct console_s *);

void console_update(struct console_s *, struct engine_s *, float dt);
void console_draw(struct console_s *, struct engine_s *);

void console_add_input_text(struct console_s *, char *text);
void console_add_message(struct console_s *, struct console_msg_s msg);

#endif

