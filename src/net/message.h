#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>
#include <stdint.h>
#include <cJSON.h>

enum message_type {
	MSG_UNKNOWN = 0,
	// lobby related
	LOBBY_CREATE_REQUEST, LOBBY_CREATE_RESPONSE,
	LOBBY_JOIN_REQUEST, LOBBY_JOIN_RESPONSE,
};

struct message_header {
	uint16_t type;
};

void message_header_init(struct message_header *, int16_t type);
void pack_message_header(struct message_header *, cJSON *json);
void unpack_message_header(cJSON *json, struct message_header *);


// LOBBY_CREATE_REQUEST
struct lobby_create_request {
	struct message_header header;
	int lobby_id;
	char *lobby_name;
};

void pack_lobby_create_request(struct lobby_create_request *, cJSON *json);
void unpack_lobby_create_request(cJSON *json, struct lobby_create_request *);

// LOBBY_CREATE_RESPONSE
struct lobby_create_response {
	struct message_header header;
	int lobby_id;
	int create_error;
};

void pack_lobby_create_response(struct lobby_create_response *, cJSON *json);
void unpack_lobby_create_response(cJSON *json, struct lobby_create_response *);

// LOBBY_JOIN_REQUEST
struct lobby_join_request {
	struct message_header header;
	int lobby_id;
};

void pack_lobby_join_request(struct lobby_join_request *, cJSON *json);
void unpack_lobby_join_request(cJSON *json, struct lobby_join_request *);

// LOBBY_JOIN_RESPONSE
struct lobby_join_response {
	struct message_header header;
	int lobby_id;
	int join_error;
};

void pack_lobby_join_response(struct lobby_join_response *msg, cJSON *json);
void unpack_lobby_join_response(cJSON *json, struct lobby_join_response *msg);

#endif

