#ifndef SERVICES_H
#define SERVICES_H

//
// forward declarations
//

struct gameserver;
struct session;
struct message_header;

struct lobby_create_request;
struct lobby_join_request;
struct lobby_list_request;


//
// api
//

void services_dispatcher(struct gameserver *gs, struct message_header *message, struct session *session);

#endif

