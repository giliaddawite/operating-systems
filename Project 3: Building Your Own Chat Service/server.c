#include "server.h"

int chat_serv_sock_fd; //server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////


char const *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

struct node *head = NULL;        // all connected users
struct room *room_head = NULL;   // all rooms and their members
struct dm   *dm_head = NULL;     // all direct connections


/////////////////////////////////////////////
// Readers-writers locking
//
// Many readers may hold the data at once; a writer needs it alone.
// mutex protects numReaders. rw_lock is held by the group of readers
// (first reader in locks it, last reader out unlocks it) or by one writer.

void reader_lock() {
   pthread_mutex_lock(&mutex);
   numReaders++;
   if(numReaders == 1)
      pthread_mutex_lock(&rw_lock);   // first reader blocks writers
   pthread_mutex_unlock(&mutex);
}

void reader_unlock() {
   pthread_mutex_lock(&mutex);
   numReaders--;
   if(numReaders == 0)
      pthread_mutex_unlock(&rw_lock); // last reader lets writers in
   pthread_mutex_unlock(&mutex);
}

void writer_lock() {
   pthread_mutex_lock(&rw_lock);
}

void writer_unlock() {
   pthread_mutex_unlock(&rw_lock);
}
/////////////////////////////////////////////


int main(int argc, char **argv) {

   signal(SIGINT, sigintHandler);

   //////////////////////////////////////////////////////
   // create the default room for all clients to join when
   // initially connecting
   //////////////////////////////////////////////////////
   writer_lock();
   room_head = insertR(room_head, DEFAULT_ROOM);
   writer_unlock();

   // Open server socket
   chat_serv_sock_fd = get_server_socket();

   // step 3: get ready to accept connections
   if(start_server(chat_serv_sock_fd, BACKLOG) == -1) {
      printf("start server error\n");
      exit(1);
   }

   printf("Server Launched! Listening on PORT: %d\n", PORT);

   // Client threads are created with SIGINT blocked, so Ctrl-C is always
   // handled by this main thread, which never holds a lock. This keeps the
   // handler from deadlocking on a lock held by the thread it interrupted.
   sigset_t sigint_set;
   sigemptyset(&sigint_set);
   sigaddset(&sigint_set, SIGINT);

   //Main execution loop
   while(1) {
      //Accept a connection, start a thread
      int new_client = accept_client(chat_serv_sock_fd);
      if(new_client != -1) {
         // give each thread its own copy of the socket number; passing
         // &new_client would race with the next accept()
         int *client_sock = malloc(sizeof(int));
         *client_sock = new_client;

         pthread_t new_client_thread;
         pthread_sigmask(SIG_BLOCK, &sigint_set, NULL);
         if(pthread_create(&new_client_thread, NULL, client_receive, (void *)client_sock) == 0)
            pthread_detach(new_client_thread);   // thread cleans itself up on exit
         else {
            perror("pthread_create");
            close(new_client);
            free(client_sock);
         }
         pthread_sigmask(SIG_UNBLOCK, &sigint_set, NULL);
      }
   }

   close(chat_serv_sock_fd);
}


int get_server_socket(char *hostname, char *port) {
    int opt = TRUE;
    int master_socket;
    struct sockaddr_in address;

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
          sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

   return master_socket;
}


int start_server(int serv_socket, int backlog) {
   int status = 0;
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return status;
}


int accept_client(int serv_sock) {
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;

   // accept a connection request from a client
   // the returned file descriptor from accept will be used
   // to communicate with this client.
   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   return reply_sock_fd;
}


/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
   char msg[MAXBUFF];
   struct node *u;

   printf("\nCtrl-C received: shutting down in %d seconds.\n", SHUTDOWN_DELAY);

   // Stretch goal: warn every connected user first
   snprintf(msg, sizeof(msg),
            "\n*** Server is going down in %d seconds. ***\nchat>", SHUTDOWN_DELAY);
   reader_lock();
   for(u = head; u != NULL; u = u->next)
      send(u->socket, msg, strlen(msg), 0);
   reader_unlock();

   sleep(SHUTDOWN_DELAY);

   //////////////////////////////////////////////////////////
   //Closing client sockets and freeing memory from user list
   //////////////////////////////////////////////////////////
   printf("--------CLOSING ACTIVE USERS--------\n");

   writer_lock();   // no client thread can touch the lists from here on

   snprintf(msg, sizeof(msg), "\n*** Server shutting down. Goodbye! ***\n");
   for(u = head; u != NULL; u = u->next) {
      printf("closing %s (socket %d)\n", u->username, u->socket);
      send(u->socket, msg, strlen(msg), 0);
      shutdown(u->socket, SHUT_RDWR);
      close(u->socket);
   }

   freeU(head);         head = NULL;
   freeR(room_head);    room_head = NULL;
   freeDM(dm_head);     dm_head = NULL;

   close(chat_serv_sock_fd);
   printf("All users disconnected, memory freed. Server stopped.\n");
   exit(0);   // the process ends while still holding the lock, so no thread runs again
}