#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>
#include <stdint.h>
#include <cJSON.h>

enum message_type {
	MSG_UNKNOWN = 0, // TODO: rename to MSG_TYPE_UNKOWN
	// lobby related
	LOBBY_CREATE_REQUEST, LOBBY_CREATE_RESPONSE,
	LOBBY_JOIN_REQUEST, LOBBY_JOIN_RESPONSE,
	LOBBY_LIST_REQUEST, LOBBY_LIST_RESPONSE,
	//
	MSG_TYPE_MAX,
};

struct message_header {
	uint16_t type; // TODO: change to enum message_type
};

typedef void (*message_pack_fn)(struct message_header *, cJSON *);
typedef void (*message_unpack_fn)(cJSON *, struct message_header *);

typedef struct {
	const char *name;
	message_pack_fn   pack_fn;
	message_unpack_fn unpack_fn;
	size_t struct_size;
} message_function_info_t;

extern const message_function_info_t message_function_infos[MSG_TYPE_MAX];

const char *message_type_to_name(enum message_type type);

void message_header_init(struct message_header *, int16_t type);
void pack_message_header(struct message_header *, cJSON *json);
void unpack_message_header(cJSON *json, struct message_header *);

/**
 * Parses the packed message into the messages respective struct.
 * Returns a pointer to the derived message type.[1]
 * Returns NULL on failure.
 *
 * Example:
 *
 *     cJSON *json = ...;
 *     struct message_header *msg_header = unpack_message(json);
 *     assert(msg_header != NULL);
 *
 *     if (msg_header->type == LOBBY_CREATE_REQUEST) {
 *       struct lobby_create_request *msg = msg_header;
 *       printf("Message Data: %d, %d, %d\n", msg->some_data, msg->more_data, ...);
 *     }
 *     free_message(json, msg_header);
 */
struct message_header *unpack_message(cJSON *json);
/**
 * Serializes a message struct and returns it as a cJSON object.
 * @param msg A message struct.
 * @returns The message serialized into a cJSON object. Ne
 *          Needs to be freed using `cJSON_Delete()`!
 */
cJSON *pack_message(struct message_header *msg);
void free_message(cJSON *json, struct message_header *);


#define MESSAGE_DECLARATION(_type, _name, _fields) \
	struct _name {                                 \
		struct message_header header;              \
		_fields;                                   \
	};                                             \
	void _name##_init(struct _name *);             \
	void _name##_destroy(struct _name *);          \
	void pack_##_name(struct _name *, cJSON *);    \
	void unpack_##_name(cJSON *, struct _name *)


MESSAGE_DECLARATION(LOBBY_CREATE_REQUEST, lobby_create_request,
	int lobby_id; char *lobby_name);

MESSAGE_DECLARATION(LOBBY_CREATE_RESPONSE, lobby_create_response,
	int lobby_id; int create_error);

MESSAGE_DECLARATION(LOBBY_JOIN_REQUEST, lobby_join_request,
	int lobby_id);

MESSAGE_DECLARATION(LOBBY_JOIN_RESPONSE, lobby_join_response,
	int lobby_id; int join_error);

MESSAGE_DECLARATION(LOBBY_LIST_REQUEST, lobby_list_request,
	void);

MESSAGE_DECLARATION(LOBBY_LIST_RESPONSE, lobby_list_response,
	int ids_of_lobbies_len; int ids_of_lobbies[8]);

#undef MESSAGE_DECLARATION

#endif

