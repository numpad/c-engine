#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

struct message_header_s {
	uint16_t type;
	uint16_t size;
};

struct message_s {
	struct message_header_s header;
	uint8_t *body;
};

struct message_s message_new(uint16_t type, uint16_t size);

#endif

