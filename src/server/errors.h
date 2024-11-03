#ifndef SERVER_ERROR_H
#define SERVER_ERROR_H

enum server_error {
	SERVER_NO_ERROR,
	// lobbies
	SERVER_ERROR_ALREADY_IN_A_LOBBY,
	SERVER_ERROR_LOBBY_ALREADY_EXISTS,
	SERVER_ERROR_LOBBY_DOES_NOT_EXIST,
	// ...
	SERVER_ERROR_UNKNOWN
};

const char *server_error_description(enum server_error);

#endif

