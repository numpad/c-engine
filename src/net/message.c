#include "message.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <cJSON.h>

// Message Header
const char *message_type_to_name(enum message_type type) {
	switch (type) {
	case LOBBY_CREATE_REQUEST:  return "LOBBY_CREATE_REQUEST";
	case LOBBY_CREATE_RESPONSE: return "LOBBY_CREATE_RESPONSE";
	case LOBBY_JOIN_REQUEST:    return "LOBBY_JOIN_REQUEST";
	case LOBBY_JOIN_RESPONSE:   return "LOBBY_JOIN_RESPONSE";
	case MSG_UNKNOWN:           return "MSG_UNKNOWN";
	}

	return NULL;
}

void message_header_init(struct message_header *msg, int16_t type) {
	msg->type = type;
}

void pack_message_header(struct message_header *msg, cJSON *json) {
	cJSON *header = cJSON_AddObjectToObject(json, "header");
	cJSON_AddNumberToObject(header, "type", msg->type);
}

void unpack_message_header(cJSON *json, struct message_header *msg) {
	cJSON *header = cJSON_GetObjectItem(json, "header");
	msg->type = cJSON_GetObjectItem(header, "type")->valueint;
}

struct message_header *get_message(cJSON *json) {
	if (json == NULL) {
		return NULL;
	}

	struct message_header header;
	unpack_message_header(json, &header);
	if (header.type == 0) {
		return NULL;
	}

	switch ((enum message_type)header.type) {
		case LOBBY_CREATE_REQUEST: {
			struct lobby_create_request *req = malloc(sizeof(*req));
			unpack_lobby_create_request(json, req);
			return (struct message_header *)req;
		}
		case LOBBY_CREATE_RESPONSE: {
			struct lobby_create_response *res = malloc(sizeof(*res));
			unpack_lobby_create_response(json, res);
			return (struct message_header *)res;
		}
		case LOBBY_JOIN_REQUEST: {
			struct lobby_join_request *req = malloc(sizeof(*req));
			unpack_lobby_join_request(json, req);
			return (struct message_header *)req;
		}
		case LOBBY_JOIN_RESPONSE: {
			struct lobby_join_response *res = malloc(sizeof(*res));
			unpack_lobby_join_response(json, res);
			return (struct message_header *)res;
		}
		case MSG_UNKNOWN:
			break;
	}

	return NULL;
}

void free_message(cJSON *json, struct message_header *msg) {
	free(msg);
	cJSON_Delete(json);
}


// LOBBY_CREATE_REQUEST
void pack_lobby_create_request(struct lobby_create_request *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_CREATE_REQUEST);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddStringToObject(json, "lobby_name", msg->lobby_name);
}

void unpack_lobby_create_request(cJSON *json, struct lobby_create_request *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_CREATE_REQUEST);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->lobby_name = cJSON_GetObjectItem(json, "lobby_name")->valuestring;
}


// LOBBY_CREATE_RESPONSE
void pack_lobby_create_response(struct lobby_create_response *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_CREATE_RESPONSE);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddNumberToObject(json, "create_error", msg->create_error);
}

void unpack_lobby_create_response(cJSON *json, struct lobby_create_response *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_CREATE_RESPONSE);
	
	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->create_error = cJSON_GetObjectItem(json, "create_error")->valueint;
}


// LOBBY_JOIN_REQUEST
void pack_lobby_join_request(struct lobby_join_request *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_JOIN_REQUEST);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
}

void unpack_lobby_join_request(cJSON *json, struct lobby_join_request *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_JOIN_REQUEST);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
}


// LOBBY_JOIN_RESPONSE
void pack_lobby_join_response(struct lobby_join_response *msg, cJSON *json) {
	assert(msg->header.type == LOBBY_JOIN_RESPONSE);
	pack_message_header(&msg->header, json);

	cJSON_AddNumberToObject(json, "lobby_id", msg->lobby_id);
	cJSON_AddNumberToObject(json, "join_error", msg->join_error);
}

void unpack_lobby_join_response(cJSON *json, struct lobby_join_response *msg) {
	unpack_message_header(json, &msg->header);
	assert(msg->header.type == LOBBY_JOIN_RESPONSE);

	msg->lobby_id = cJSON_GetObjectItem(json, "lobby_id")->valueint;
	msg->join_error = cJSON_GetObjectItem(json, "join_error")->valueint;
}

