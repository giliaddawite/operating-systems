#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define NAMELEN 30

/////////////////// USERLIST //////////////////////////

struct node {
   char username[NAMELEN];
   int socket;
   struct node *next;
};

//insert node at the first location
struct node* insertFirstU(struct node *head, int socket, char *username);

//find a node with given username
struct node* findU(struct node *head, char* username);

//find a node with given socket
struct node* findU_sock(struct node *head, int socket);

//remove the node with given socket, returns new head
struct node* removeU(struct node *head, int socket);

//free the whole user list
void freeU(struct node *head);


/////////////////// ROOMLIST //////////////////////////

// a room member is referenced by socket, so renaming a user (login)
// automatically shows up in every room
struct member {
   int socket;
   struct member *next;
};

struct room {
   char name[NAMELEN];
   struct member *members;
   struct room *next;
};

//insert a new room at the first location (ignored if it already exists)
struct room* insertR(struct room *head, char *name);

//find a room with given name
struct room* findR(struct room *head, char *name);

//add socket to room, returns 0 on success, -1 if already a member
int addMember(struct room *r, int socket);

//remove socket from room, returns 0 on success, -1 if not a member
int removeMember(struct room *r, int socket);

//true if socket is a member of room
bool isMember(struct room *r, int socket);

//remove socket from every room (used on exit)
void removeFromAllRooms(struct room *head, int socket);

//true if sockets a and b are both in at least one common room
bool shareRoom(struct room *head, int a, int b);

//free all rooms and their member lists
void freeR(struct room *head);


/////////////////// DIRECT CONNECTIONS (DM) ///////////

// a DM is a link between two users (by socket), order does not matter
struct dm {
   int sock_a;
   int sock_b;
   struct dm *next;
};
//insert a DM link at the first location (ignored if it already exists)
struct dm* insertDM(struct dm *head, int a, int b);

//true if a and b are directly connected
bool isDM(struct dm *head, int a, int b);

//remove the DM link between a and b, returns new head
struct dm* removeDM(struct dm *head, int a, int b);

//remove every DM link involving socket (used on exit), returns new head
struct dm* removeAllDM(struct dm *head, int socket);

//free the whole DM list
void freeDM(struct dm *head);