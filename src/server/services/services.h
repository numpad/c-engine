#ifndef SERVICES_H
#define SERVICES_H

//
// forward declarations
//

struct gameserver;
struct session;

struct lobby_create_request;
struct lobby_join_request;
struct lobby_list_request;


//
// api
//

// group service
void create_lobby(struct gameserver *gserver, struct lobby_create_request *msg, struct session *requested_by);
void join_lobby  (struct gameserver *gserver, struct lobby_join_request *msg, struct session *requested_by);
void list_lobbies(struct gameserver *gserver, struct lobby_list_request *msg, struct session *requested_by);

#endif

