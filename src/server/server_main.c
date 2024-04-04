#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stb_ds.h>
#include "gameserver.h"
#include "net/message.h"
#include "services/services.h"

//
// logic
//

//
// functions
//

static void serverui_on_input(const char *cmd, size_t cmd_len);

static void *thread_run_gameserver(void *data);

// ui
static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void console_log(char *fmt, ...);

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
	console_log("[%s]  Server started on %s", fmt_time(time(NULL)), fmt_date(time(NULL)));
	console_log("[%s]  PID : %ld\n", fmt_time(time(NULL)), (long)getpid());

	// start bg services
	pthread_t gameserver_thread;
	pthread_create(&gameserver_thread, NULL, thread_run_gameserver, NULL);

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

	pthread_join(gameserver_thread, NULL);
	pthread_mutex_destroy(&messagequeue_mutex);

	delwin(win_messages);
	delwin(win_input);
	endwin();

	return 0;
}

//
// server logic
//

static void server_on_connect(struct gameserver *gs, struct session *session) {
	console_log("Client %d connected.", session->id);

	// send a welcome message
	struct welcome_response res;
	message_header_init(&res.header, WELCOME_RESPONSE);
	res._dummy = 1337;
	gameserver_send_to(gs, (struct message_header *)&res, session);
}

static void server_on_disconnect(struct gameserver *gs, struct session *session) {
	console_log("Client %d disconnected.", session->id);
}

static void server_on_message(struct gameserver *gs, struct session *session, struct message_header *message) {
	console_log("Received %s from #%06d", message_type_to_name(message->type), session->id);
	services_dispatcher(gs, message, session);
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
		console_log("%s", input);
		/*
		if (strncmp(input, "/queue", strlen("/queue")) == 0) {
			for (int i = 0; i < stbds_arrlen(sessions); ++i) {
				struct session *s = sessions[i];
				if (s->msg_queue != NULL) {
					console_log("Outstanding messages for client %d:", s->id);
					for (int j = 0; j < stbds_arrlen(s->msg_queue); ++j) {
						console_log(" - %s", s->msg_queue[i]);
					}
				} else {
					console_log("No Message Queue for %d:", s->id);
				}
			}
		}

		if (strncmp(input, "/sessions", strlen("/sessions")) == 0) {
			const int sessions_len = stbds_arrlen(sessions);
			for (int i = 0; i < sessions_len; ++i) {
				const struct session *s = sessions[i];
				console_log("Session #%d (%d): In Lobby: %d", i, s->id, s->group_id);
			}
			console_log("Total: %d Sessions", sessions_len);
		}
		*/

	} else {
		if (input_len > 1) {
			// TODO: remove. just sends the raw input for debugging purposes.
			gameserver_send_raw(&gserver, NULL, (uint8_t *)input, input_len);
			console_log("[%s] %s", fmt_time(time(NULL)), input);
		}
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

static void console_log(char *fmt, ...) {
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

static void *thread_run_gameserver(void *data) {
	const int port = 9124;

	// init server
	if (gameserver_init(&gserver, port)) {
		console_log("Failed starting Websocket server!");
		return (void *)1;
	}

	// register callbacks
	gserver.callback_on_connect = server_on_connect;
	gserver.callback_on_disconnect = server_on_disconnect;
	gserver.callback_on_message = server_on_message;

	// run
	console_log("Starting Websocket server on :%d...", port);
	gameserver_listen(&gserver);

	// destroy
	gameserver_destroy(&gserver);

	return (void *)0;
}

//
// game logic
//
