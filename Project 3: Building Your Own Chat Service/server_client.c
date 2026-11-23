#include "server.h"

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
// (wrapped by reader_lock / reader_unlock / writer_lock / writer_unlock in server.c)
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern struct node *head;        // users
extern struct room *room_head;   // rooms
extern struct dm   *dm_head;     // direct connections

extern char const *server_MOTD;


char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

// send a null-terminated string to one socket
static void send_str(int sock, const char *str) {
   send(sock, str, strlen(str), 0);
}

// remove a user from every room, every DM and the user list, then close the socket
static void remove_client(int client) {
   writer_lock();
   removeFromAllRooms(room_head, client);
   dm_head = removeAllDM(dm_head, client);
   head = removeU(head, client);
   writer_unlock();
   close(client);
}


/*
 *Main thread for each client.  Receives all messages
 *and passes the data off to the correct function.  Receives
 *a pointer to the file descriptor for the socket the thread
 *should listen on
 */
void *client_receive(void *ptr) {
   int client = *(int *) ptr;  // socket
   free(ptr);                  // allocated per-thread in main()

   int received, i;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  //data buffers
   char tmpbuf[MAXBUFF];                    //temp buffer
   char cmd[MAXBUFF], username[NAMELEN];
   char *arguments[80];

   struct node *currentUser;
   struct room *r;

   send_str(client, server_MOTD); // Send Welcome Message of the Day.

   // Creating the guest user name and adding the GUEST to the DEFAULT ROOM (i.e. Lobby)
   snprintf(username, sizeof(username), "guest%d", client);

   writer_lock();
   head = insertFirstU(head, client, username);
   r = findR(room_head, DEFAULT_ROOM);
   if(r != NULL)
      addMember(r, client);
   writer_unlock();
   printf("%s connected and joined %s\n", username, DEFAULT_ROOM);

   while (1) {

      received = read(client, buffer, MAXBUFF - 1);
      if(received <= 0) {
         // client closed telnet (0) or the connection failed (-1)
         printf("socket %d disconnected\n", client);
         remove_client(client);
         return NULL;
      }

      buffer[received] = '\0';
      strcpy(cmd, buffer);
      strcpy(sbuffer, trimwhitespace(buffer));   // message text without \r\n

      /////////////////////////////////////////////////////
      // we got some data from a client

      // 1. Tokenize the input in buf (split it on whitespace)
      arguments[0] = strtok(cmd, delimiters);
      i = 0;
      while(arguments[i] != NULL && i < 78) {
         arguments[i] = trimwhitespace(arguments[i]);
         arguments[++i] = strtok(NULL, delimiters);
      }
      arguments[i] = NULL;

      // ignore blank lines
      if(arguments[0] == NULL || arguments[0][0] == '\0') {
         send_str(client, "chat>");
         continue;
      }

      // Arg[0] = command
      // Arg[1] = user or room
      int has_arg = (arguments[1] != NULL && arguments[1][0] != '\0');

      /////////////////////////////////////////////////////
      // 2. Execute command

      if(strcmp(arguments[0], "create") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: create <room>\nchat>");
            continue;
         }
         printf("create room: %s\n", arguments[1]);

         writer_lock();
         if(findR(room_head, arguments[1]) != NULL)
            snprintf(buffer, MAXBUFF, "room %s already exists\nchat>", arguments[1]);
         else {
            room_head = insertR(room_head, arguments[1]);
            snprintf(buffer, MAXBUFF, "room %s created\nchat>", arguments[1]);
         }
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "join") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: join <room>\nchat>");
            continue;
         }
         printf("join room: %s\n", arguments[1]);

         writer_lock();
         r = findR(room_head, arguments[1]);
         if(r == NULL)
            snprintf(buffer, MAXBUFF, "room %s does not exist\nchat>", arguments[1]);
         else if(addMember(r, client) == -1)
            snprintf(buffer, MAXBUFF, "you are already in %s\nchat>", arguments[1]);
         else
            snprintf(buffer, MAXBUFF, "joined room %s\nchat>", arguments[1]);
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "leave") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: leave <room>\nchat>");
            continue;
         }
         printf("leave room: %s\n", arguments[1]);

         writer_lock();
         r = findR(room_head, arguments[1]);
         if(r == NULL)
            snprintf(buffer, MAXBUFF, "room %s does not exist\nchat>", arguments[1]);
         else if(removeMember(r, client) == -1)
            snprintf(buffer, MAXBUFF, "you are not in %s\nchat>", arguments[1]);
         else
            snprintf(buffer, MAXBUFF, "left room %s\nchat>", arguments[1]);
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "connect") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: connect <user>\nchat>");
            continue;
         }
         printf("connect to user: %s \n", arguments[1]);

         writer_lock();
         currentUser = findU(head, arguments[1]);
         if(currentUser == NULL)
            snprintf(buffer, MAXBUFF, "user %s not found\nchat>", arguments[1]);
         else if(currentUser->socket == client)
            snprintf(buffer, MAXBUFF, "you cannot connect to yourself\nchat>");
         else if(isDM(dm_head, client, currentUser->socket))
            snprintf(buffer, MAXBUFF, "already connected to %s\nchat>", arguments[1]);
         else {
            dm_head = insertDM(dm_head, client, currentUser->socket);
            snprintf(buffer, MAXBUFF, "connected to %s\nchat>", arguments[1]);
         }
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "disconnect") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: disconnect <user>\nchat>");
            continue;
         }
         printf("disconnect from user: %s\n", arguments[1]);

         writer_lock();
         currentUser = findU(head, arguments[1]);
         if(currentUser == NULL)
            snprintf(buffer, MAXBUFF, "user %s not found\nchat>", arguments[1]);
         else if(!isDM(dm_head, client, currentUser->socket))
            snprintf(buffer, MAXBUFF, "you are not connected to %s\nchat>", arguments[1]);
         else {
            dm_head = removeDM(dm_head, client, currentUser->socket);
            snprintf(buffer, MAXBUFF, "disconnected from %s\nchat>", arguments[1]);
         }
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "rooms") == 0)
      {
         printf("List all the rooms\n");

         // build the list of rooms into buffer
         strcpy(buffer, "rooms:\n");
         reader_lock();
         for(r = room_head; r != NULL; r = r->next) {
            snprintf(tmpbuf, MAXBUFF, "  %s%s\n", r->name,
                     isMember(r, client) ? " (joined)" : "");
            if(strlen(buffer) + strlen(tmpbuf) < MAXBUFF - 8)
               strcat(buffer, tmpbuf);
         }
         reader_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "users") == 0)
      {
         printf("List all the users\n");

         // list the users you are connected to: anyone who shares a room
         // with you, or anyone you have a direct connection (DM) with
         strcpy(buffer, "users you are connected to:\n");
         int count = 0;
         reader_lock();
         for(currentUser = head; currentUser != NULL; currentUser = currentUser->next) {
            if(currentUser->socket == client)
               continue;
            int room = shareRoom(room_head, client, currentUser->socket);
            int dm   = isDM(dm_head, client, currentUser->socket);
            if(room || dm) {
               snprintf(tmpbuf, MAXBUFF, "  %s%s%s\n", currentUser->username,
                        room ? " [room]" : "", dm ? " [DM]" : "");
               if(strlen(buffer) + strlen(tmpbuf) < MAXBUFF - 16) {
                  strcat(buffer, tmpbuf);
                  count++;
               }
            }
         }
         reader_unlock();

         if(count == 0)
            strcat(buffer, "  (none)\n");
         strcat(buffer, "chat>");
         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "login") == 0)
      {
         if(!has_arg) {
            send_str(client, "usage: login <username>\nchat>");
            continue;
         }

         //rename their guestID to username. Rooms and DMs store the socket,
         //not the name, so they show the new username automatically.
         writer_lock();
         currentUser = findU(head, arguments[1]);
         if(strlen(arguments[1]) >= NAMELEN)
            snprintf(buffer, MAXBUFF, "username must be under %d characters\nchat>", NAMELEN);
         else if(strncmp(arguments[1], "guest", 5) == 0)
            snprintf(buffer, MAXBUFF, "names starting with \"guest\" are reserved\nchat>");
         else if(currentUser != NULL && currentUser->socket != client)
            snprintf(buffer, MAXBUFF, "username %s is already taken\nchat>", arguments[1]);
         else {
            currentUser = findU_sock(head, client);
            if(currentUser != NULL) {
               printf("%s logged in as %s\n", currentUser->username, arguments[1]);
               strcpy(currentUser->username, arguments[1]);
            }
            snprintf(buffer, MAXBUFF, "logged in as %s\nchat>", arguments[1]);
         }
         writer_unlock();

         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "help") == 0 )
      {
         sprintf(buffer, "login <username> - \"login with username\" \ncreate <room> - \"create a room\" \njoin <room> - \"join a room\" \nleave <room> - \"leave a room\" \nusers - \"list all users\" \nrooms -  \"list all rooms\" \nconnect <user> - \"connect to user\" \ndisconnect <user> - \"disconnect from user\" \nexit or logout - \"exit chat\" \nchat>");
         send_str(client, buffer);
      }
      else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0)
      {
         //Remove the initiating user from all rooms and direct connections, then close the socket descriptor.
         send_str(client, "Goodbye!\n");
         printf("socket %d logged out\n", client);
         remove_client(client);
         return NULL;
      }
      else {
         /////////////////////////////////////////////////////////////
         // 3. sending a message
         // send ::[userfrom]> <message> followed by chat> only to users who
         // share a room with the sender or are directly connected (DM)

         reader_lock();
         currentUser = findU_sock(head, client);
         snprintf(tmpbuf, MAXBUFF, "\n::%.*s> %.*s\nchat>",
                  NAMELEN, currentUser ? currentUser->username : "unknown",
                  MAXBUFF - NAMELEN - 16, sbuffer);

         for(currentUser = head; currentUser != NULL; currentUser = currentUser->next) {
            if(currentUser->socket == client)   // dont send to yourself
               continue;
            if(shareRoom(room_head, client, currentUser->socket) ||
               isDM(dm_head, client, currentUser->socket))
               send_str(currentUser->socket, tmpbuf);
         }
         reader_unlock();

         send_str(client, "chat>");
      }

      memset(buffer, 0, sizeof(buffer));
   }
   return NULL;
}