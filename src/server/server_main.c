#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <libwebsockets.h>
#include <stb_ds.h>
#include "gameserver.h"
#include "net/message.h"

//
// logic
//

// Group Filter
// A group filter function tests if the session `tested` is inside the `owner`s group.
// Returns 1 (true) if `tested` is in the group, 0 otherwise.
static int group_lobby(struct session *o, struct session *t) { return (t->group_id == o->group_id); }
static int group_lobby_without_owner(struct session *o, struct session *t) { return (o->id != t->id && t->group_id == o->group_id); }
static int group_everybody(struct session *o, struct session *t) { return 1; }
static int group_everybody_without_owner(struct session *o, struct session *t) { return (o->id != t->id); }

//
// functions
//

static void serverui_on_input(const char *cmd, size_t cmd_len);

void *wsserver_thread(void *data);

// ui
static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void messagequeue_add(char *fmt, ...);

// game logic
static void create_lobby(struct lobby_create_request *msg, struct session *);
static void join_lobby(struct lobby_join_request *msg, struct session *session);
static void list_lobbies(struct lobby_list_request *msg, struct session *requested_by);

//
// vars
//

static pthread_mutex_t messagequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t messagequeue_max = 128;
static char *messagequeue[128];
static size_t messagequeue_len = 0;
static struct gameserver gserver;

int main(int argc, char **argv) {
	initscr();
	cbreak();
	noecho();
	//keypad(stdscr, TRUE);

	WINDOW *win_messages = create_messages_window();
	WINDOW *win_input = create_input_window();

	wtimeout(win_input, 100);
	//nodelay(stdscr, TRUE);
	//wtimeout(win_messages, 100);
	wmove(win_messages, 0, 0);
	messagequeue_add("[%s]  Server started on %s", fmt_time(time(NULL)), fmt_date(time(NULL)));
	messagequeue_add("[%s]  PID : %ld\n", fmt_time(time(NULL)), (long)getpid());

	// start bg services
	pthread_t ws_thread;
	pthread_create(&ws_thread, NULL, wsserver_thread, NULL);

	int running = 1;
	while (running) {
		static char in_command[256] = {0};
		static size_t in_command_i = 0;

		// logic
		{
			pthread_mutex_lock(&messagequeue_mutex);

			size_t i = 0;
			while (i < messagequeue_len) {
				wprintw(win_messages, "%s\n", messagequeue[i]);
				free(messagequeue[i]);
				++i;
			}
			messagequeue_len = 0;

			pthread_mutex_unlock(&messagequeue_mutex);
		}

		// draw
		wattron(win_input, A_BOLD);
		mvwprintw(win_input, 0, 0, "command: ");
		wattroff(win_input, A_BOLD);
		wprintw(win_input, "%.*s", (int)in_command_i, in_command);
		refresh();
		wrefresh(win_messages);
		wrefresh(win_input);

		// read input
		int in_char = wgetch(win_input);
		if (in_char == 127) {
			if (in_command_i > 0) {
				--in_command_i;
			}
			werase(win_input);
		} else if (in_char == '') {
			werase(win_messages);
		} else if (in_char == 23 /* ^W */) {
			in_command_i = 0;
			werase(win_input);
		} else if (in_char == 4 /* ^D */) {
			running = 0;
		} else if (in_char == '\n') {
			serverui_on_input(in_command, in_command_i);
			werase(win_input);
			in_command_i = 0;
		} else if (in_char != ERR) {
			in_command[in_command_i++] = in_char;
		}
	}

	pthread_join(ws_thread, NULL);
	pthread_mutex_destroy(&messagequeue_mutex);

	delwin(win_messages);
	delwin(win_input);
	endwin();

	return 0;
}

//
// server logic
//

static void server_on_message(struct session *session, struct message_header *msg_header) {

	// we got a fully valid message.
	messagequeue_add("Received %s from #%06d", message_type_to_name(msg_header->type), session->id);
	switch (msg_header->type) {
		case LOBBY_CREATE_REQUEST:
			create_lobby((struct lobby_create_request *)msg_header, session);
			break;
		case LOBBY_JOIN_REQUEST:
			join_lobby((struct lobby_join_request *)msg_header, session);
			break;
		case LOBBY_LIST_REQUEST: {
			list_lobbies((struct lobby_list_request *)msg_header, session);
			break;
		}
		case LOBBY_CREATE_RESPONSE:
		case LOBBY_JOIN_RESPONSE:
		case LOBBY_LIST_RESPONSE:
			// these messages we cant handle
		case MSG_TYPE_UNKNOWN:
		case MSG_TYPE_MAX:
			// these messages are invalid
			break;
	}
}

//
// server ui
//

static void serverui_on_input(const char *cmd, size_t cmd_len) {
	// input to c string
	int input_len = snprintf(NULL, 0, "%.*s", (int)cmd_len, cmd);
	char input[input_len + 1];
	snprintf(input, input_len + 1, "%.*s", (int)cmd_len, cmd);
	
	int is_command = (input[0] == '/');
	if (is_command) {
		messagequeue_add("%s", input);
		/*
		if (strncmp(input, "/queue", strlen("/queue")) == 0) {
			for (int i = 0; i < stbds_arrlen(sessions); ++i) {
				struct session *s = sessions[i];
				if (s->msg_queue != NULL) {
					messagequeue_add("Outstanding messages for client %d:", s->id);
					for (int j = 0; j < stbds_arrlen(s->msg_queue); ++j) {
						messagequeue_add(" - %s", s->msg_queue[i]);
					}
				} else {
					messagequeue_add("No Message Queue for %d:", s->id);
				}
			}
		}

		if (strncmp(input, "/sessions", strlen("/sessions")) == 0) {
			const int sessions_len = stbds_arrlen(sessions);
			for (int i = 0; i < sessions_len; ++i) {
				const struct session *s = sessions[i];
				messagequeue_add("Session #%d (%d): In Lobby: %d", i, s->id, s->group_id);
			}
			messagequeue_add("Total: %d Sessions", sessions_len);
		}
		*/

	} else {
		// TODO: remove. just sends the raw input for debugging purposes.
		gameserver_send_raw(&gserver, NULL, (uint8_t *)input, input_len);
		messagequeue_add("[%s] %s", fmt_time(time(NULL)), input);
	}
}

static WINDOW *create_messages_window(void) {
	WINDOW *win = newwin(LINES, COLS, 0, 0);
	scrollok(win, TRUE);

	return win;
}

static WINDOW *create_input_window(void) {
	WINDOW *win = newwin(1, COLS, LINES - 1, 0);
	return win;
}

static const char *fmt_time(time_t rawtime) {
	// Get current time
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);

	static char buffer[9]; // HH:MM:SS\0
	strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
	return buffer;
}

static const char *fmt_date(time_t rawtime) {
	// Get current time
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);

	static char buffer[11]; // YYYY-MM-DD\0
	strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
	return buffer;
}

static void messagequeue_add(char *fmt, ...) {
	va_list args;
	
	// determine size
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	char *buf = malloc((len + 1) * sizeof(char));
	buf[len] = '\0';
	va_end(args);

	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	// write to queue
	pthread_mutex_lock(&messagequeue_mutex);
	assert(messagequeue_len < messagequeue_max);
	messagequeue[messagequeue_len++] = buf;
	pthread_mutex_unlock(&messagequeue_mutex);

}

//
// server impls
//

void *wsserver_thread(void *data) {
	const int port = 9124;

	// init server
	if (gameserver_init(&gserver, port)) {
		messagequeue_add("Failed starting Websocket server!");
		return (void *)1;
	}

	// register callbacks
	gserver.callback_on_message = server_on_message;

	messagequeue_add("Starting Websocket server on :%d...", port);
	
	// run
	gameserver_listen(&gserver);

	// destroy
	gameserver_destroy(&gserver);

	return (void *)0;
}

//
// game logic
//

/* Creates a new lobby and moves `requested_by` into it.
 * @param msg The requests data.
 * @param session The session which requested this. Can be NULL, if the server creates the lobby for example.
 */
static void create_lobby(struct lobby_create_request *msg, struct session *requested_by) {
	messagequeue_add("[create_lobby] Session Ptr: %p", requested_by);
	assert(msg != NULL);
	assert(requested_by != NULL); // TODO: also support no session
	assert(msg->lobby_id >= 0); // TODO: this is application logic...

	struct lobby_create_response response;
	message_header_init(&response.header, LOBBY_CREATE_RESPONSE);
	response.lobby_id = msg->lobby_id;
	response.create_error = 0;

	if (requested_by->group_id > 0) {
		messagequeue_add("#%06d is already in lobby %d but requested to create %d", requested_by->id, requested_by->group_id, msg->lobby_id);
		response.create_error = 1;
		gameserver_send_to(&gserver, &response.header, requested_by);

		return;
	}

	// TODO: check if lobby already exists

	response.create_error = 0;
	requested_by->group_id = msg->lobby_id;
	messagequeue_add("#%06d created, and joined lobby %d!", requested_by->id, requested_by->group_id);

	gameserver_send_to(&gserver, &response.header, requested_by);

	// also send join response
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	join.is_other_user = 0;
	join.join_error = 0;
	join.lobby_id = requested_by->group_id;
	gameserver_send_to(&gserver, &join.header, requested_by);

	// also notify all others
	struct lobby_create_response create;
	message_header_init(&create.header, LOBBY_CREATE_RESPONSE);
	create.create_error = 0;
	create.lobby_id = msg->lobby_id;
	gameserver_send_filtered(&gserver, &create.header, requested_by, group_everybody_without_owner);
}

static void join_lobby(struct lobby_join_request *msg, struct session *requested_by) {
	
	struct lobby_join_response join;
	message_header_init(&join.header, LOBBY_JOIN_RESPONSE);
	join.is_other_user = 0;
	join.join_error = 0;
	join.lobby_id = 0;
	
	// if already in a lobby, leave.
	const int is_already_in_lobby = (requested_by->group_id > 0);
	if (is_already_in_lobby && msg->lobby_id != 0) {
		messagequeue_add("Cannot join %d, Already in lobby %d", msg->lobby_id, requested_by->group_id);
		join.join_error = 1; // can only LEAVE (-1) while in lobby
		join.lobby_id = requested_by->group_id; // shouldn't matter, just to be sure.
		goto send_response;
	}

	// check if the lobby exists.
	int lobby_exists = (msg->lobby_id == 0); // "0" is indicates a leave and always "exists".
	if (lobby_exists == 0) {
		const size_t sessions_len = stbds_arrlen(gserver.sessions);
		for (size_t i = 0; i < sessions_len; ++i) {
			struct session *s = gserver.sessions[i];
			if (msg->lobby_id == s->group_id) {
				lobby_exists = 1;
				break;
			}
		}
	}

	// lobby doesnt exist? then we cant join.
	if (lobby_exists == 0) {
		join.join_error = 2; // INFO: lobby does not exist.
		join.lobby_id = 0;
		goto send_response;
	}

	if (msg->lobby_id != 0) {
		requested_by->group_id = msg->lobby_id;
	}

	// lobby_id can be 0 to indicate a leave
	join.join_error = 0;
	join.lobby_id = msg->lobby_id;

send_response:
	gameserver_send_to(&gserver, &join.header, requested_by);

	if (join.join_error == 0) {
		join.is_other_user = 1;
		messagequeue_add("Sending to group...");
		gameserver_send_filtered(&gserver, &join.header, requested_by, group_lobby_without_owner);
	}

	if (msg->lobby_id == 0) {
		requested_by->group_id = msg->lobby_id;
	}
}

/** Sends a list of all lobbies to `requested_by`.
 * @param msg The request data.
 * @param requested_by The client who requested the list.
 */
static void list_lobbies(struct lobby_list_request *msg, struct session *requested_by) {
	int sessions_len = stbds_arrlen(gserver.sessions);
	if (sessions_len > 8) sessions_len = 8;

	struct lobby_list_response res;
	message_header_init(&res.header, LOBBY_LIST_RESPONSE);
	res.ids_of_lobbies_len = 0;
	for (int i = 0; i < sessions_len; ++i) {
		int lobby_id = gserver.sessions[i]->group_id;
		
		// check if we already added lobby_id to list.
		for (int j = 0; j < res.ids_of_lobbies_len; ++j) {
			if (lobby_id == res.ids_of_lobbies[j]) {
				lobby_id = 0; // already exists
			}
		}

		if (lobby_id > 0) {
			res.ids_of_lobbies[res.ids_of_lobbies_len] = lobby_id;
			++res.ids_of_lobbies_len;
		}
	}
	gameserver_send_to(&gserver, &res.header, requested_by);
}

