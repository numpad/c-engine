#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <stdint.h>
#include <stddef.h>

//
// forward declarations
//

struct lws;
struct lws_context;
struct session;
struct message_header;
struct gameserver;

//
// typedefs
//

typedef void (*on_connect_fn)(struct session *);
typedef void (*on_disconnect_fn)(struct session *);
typedef void (*on_message_fn)(struct session *, struct message_header *message);

typedef int (*session_filter_fn)(struct session *master, struct session *tested);

//
// structs
//

struct gameserver {
	struct lws_context *lws;
	struct session **sessions;

	// callbacks
	on_connect_fn    callback_on_connect;
	on_disconnect_fn callback_on_disconnect;
	on_message_fn    callback_on_message;
};

/* represents the state of a client connection */
struct session {
	struct lws *wsi;
	char **message_queue;
	
	// client userdata
	int id;
	uint32_t group_id;
};

//
// api
//

// init & destroy
int  gameserver_init   (struct gameserver *, uint16_t port);
void gameserver_destroy(struct gameserver *);

// main loop
void gameserver_listen(struct gameserver *);

// sending data
void gameserver_send_raw     (struct gameserver *, struct session *receiver, uint8_t *data, size_t data_len);
void gameserver_send_to      (struct gameserver *, struct message_header *message, struct session *receiver);
void gameserver_send_filtered(struct gameserver *, struct message_header *message, struct session *master, session_filter_fn filter);

#endif

