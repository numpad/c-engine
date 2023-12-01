#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ncurses.h>
#include <SDL.h>
#include <SDL_net.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

//
// logic
//

static void on_connect(TCPsocket *client) {
	printf("Client [%p] connected!\n", (void *)client);
}

static void on_message(TCPsocket *client, const char *message, size_t message_len) {
	printf("Client sent message: %.*s!\n", (int)message_len, message);
}

static void on_disconnect(TCPsocket *client) {
	printf("Client [%p] disconnected!\n", (void *)client);
}

//
// server code
//

static int init_sdlnet(IPaddress *ip, TCPsocket socket);

void *server_thread(void *data);
void *tcpserver_thread(void *data);
void *wsserver_thread(void *data);
void *mockserver_thread(void *data);

static WINDOW *create_messages_window(void);
static WINDOW *create_input_window(void);

static const char *fmt_time(time_t time);
static const char *fmt_date(time_t rawtime);

static void messagequeue_add(char *fmt, ...);

//
// vars
//

static pthread_mutex_t messagequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
const size_t messagequeue_max = 128;
static char *messagequeue[128];
static size_t messagequeue_len = 0;

int main(int argc, char **argv) {
	initscr();
	cbreak();
	noecho();
	//keypad(stdscr, TRUE);

	WINDOW *win_messages = create_messages_window();
	WINDOW *win_input = create_input_window();

	wtimeout(win_input, 500);
	//nodelay(stdscr, TRUE);
	//wtimeout(win_messages, 100);
	wmove(win_messages, 0, 0);
	messagequeue_add("[%s]  Server started on %s", fmt_time(time(NULL)), fmt_date(time(NULL)));
	messagequeue_add("[%s]  ", fmt_time(time(NULL)));

	// start bg services
	pthread_t tcp_thread;
	pthread_t ws_thread;
	pthread_t mock_thread;
	pthread_create(&tcp_thread, NULL, tcpserver_thread, NULL);
	pthread_create(&ws_thread, NULL, wsserver_thread, NULL);
	pthread_create(&mock_thread, NULL, mockserver_thread, NULL);

	int running = 1;
	while (running) {
		static char in_command[256] = {0};
		static size_t in_command_i = 0;

		// logic
		size_t i = 0;
		pthread_mutex_lock(&messagequeue_mutex);
		while (i < messagequeue_len) {
			wprintw(win_messages, "%s\n", messagequeue[i]);
			free(messagequeue[i]);
			++i;
		}
		messagequeue_len = 0;
		pthread_mutex_unlock(&messagequeue_mutex);

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
			messagequeue_add("[%s]  %.*s\n", fmt_time(time(NULL)), (int)in_command_i, in_command);
			werase(win_input);
			in_command_i = 0;
		} else if (in_char != ERR) {
			in_command[in_command_i++] = in_char;
		}
	}

	pthread_join(tcp_thread, NULL);
	pthread_join(ws_thread, NULL);
	pthread_mutex_destroy(&messagequeue_mutex);

	delwin(win_messages);
	delwin(win_input);
	endwin();

	SDLNet_Quit();
	return 0;
}

static int init_sdlnet(IPaddress *ip, TCPsocket socket) {
	assert(ip != NULL);
	assert(socket == NULL);

	if (SDL_Init(0) < 0) {
		fprintf(stderr, "SDL_Init() failed: %s...\n", SDL_GetError());
		return 1;
	}

	if (SDLNet_Init() < 0) {
		fprintf(stderr, "SDLNet_Init() failed: %s...\n", SDLNet_GetError());
		return 1;
	}

	if (SDLNet_ResolveHost(ip, NULL, ip->port) < 0) {
		fprintf(stderr, "SDLNet_ResolveHost() failed: %s...\n", SDLNet_GetError());
		SDLNet_Quit();
		return 1;
	}

	socket = SDLNet_TCP_Open(ip);
	if (socket == NULL) {
		fprintf(stderr, "SDLNet_TCP_Open() failed: %s...\n", SDLNet_GetError());
		SDLNet_Quit();
		return 1;
	}

	return 0;
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


void *mockserver_thread(void *data) {
	int i = 10000;
	while (--i) {
		messagequeue_add("<automated mock msg>");

		sleep(1);
	}

	return NULL;
}

void *tcpserver_thread(void *data) {
	IPaddress ip;
	ip.port = 9123;
	TCPsocket socket = NULL;
	if (init_sdlnet(&ip, socket) != 0) {
		messagequeue_add("Failed initializing SDLNet...");
		return (void *)1;
	}

	messagequeue_add("SDLNet running on :%u\n", ntohs(ip.port));

	
	SDLNet_Quit();
	return (void *)0;
}

void *wsserver_thread(void *data) {
	
	// loop

	return (void *)0;
}

