#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <stdint.h>
#include <stddef.h>

//
// types
//

// forward decls
struct lws;
struct lws_context;
struct session;
struct message_header;
struct gameserver;

// function ptrs
typedef void (*on_connect_fn)   (struct gameserver *, struct session *);
typedef void (*on_disconnect_fn)(struct gameserver *, struct session *);
typedef void (*on_message_fn)   (struct gameserver *, struct session *, struct message_header *message);

typedef int (*session_filter_fn)(struct session *master, struct session *tested);

//
// enums & structs
//

enum connection_type {
	CONNECTION_TYPE_UNKNOWN,
	CONNECTION_TYPE_WEBSOCKET,
	CONNECTION_TYPE_TCP,
};

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
	enum connection_type connection_type;
	
	// client userdata
	int id;
	uint32_t group_id;
};

//
// api
//

// init & destroy
int  gameserver_init         (struct gameserver *, uint16_t port);
void gameserver_destroy      (struct gameserver *);

//   main                            loop
void gameserver_listen       (struct gameserver *);

//   sending                         data
void gameserver_send_raw     (struct gameserver *, struct session *receiver, uint8_t *data, size_t data_len);
void gameserver_send_to      (struct gameserver *, struct message_header *message, struct session  *receiver);
void gameserver_send_filtered(struct gameserver *, struct message_header *message, struct session *master, session_filter_fn filter);

// filters
int  filter_group            (struct session *o, struct session *t);
int  filter_group_exclude    (struct session *o, struct session *t);
int  filter_everybody        (struct session *o, struct session *t);
int  filter_everybody_exclude(struct session *o, struct session *t);

#endif

