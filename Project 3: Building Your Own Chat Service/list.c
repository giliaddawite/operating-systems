#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "list.h"

/////////////////// USERLIST //////////////////////////

//insert link at the first location
struct node* insertFirstU(struct node *head, int socket, char *username) {

   if(findU(head,username) == NULL) {

       //create a link
       struct node *link = (struct node*) malloc(sizeof(struct node));

       link->socket = socket;
       strncpy(link->username, username, NAMELEN - 1);
       link->username[NAMELEN - 1] = '\0';

       //point it to old first node
       link->next = head;

       //point first to new first node
       head = link;

   }
   else
       printf("Duplicate: %s\n", username);
   return head;
}

//find a link with given user
struct node* findU(struct node *head, char* username) {
   struct node *current = head;
   while(current != NULL) {
      if(strcmp(current->username, username) == 0)
         return current;
      current = current->next;
   }
   return NULL;
}

//find a link with given socket
struct node* findU_sock(struct node *head, int socket) {
   struct node *current = head;
   while(current != NULL) {
      if(current->socket == socket)
         return current;
      current = current->next;
   }
   return NULL;
}

//remove the link with given socket
struct node* removeU(struct node *head, int socket) {
   struct node *current = head, *prev = NULL;
   while(current != NULL) {
      if(current->socket == socket) {
         if(prev == NULL) head = current->next;
         else prev->next = current->next;
         free(current);
         return head;
      }
      prev = current;
      current = current->next;
   }
   return head;
}

void freeU(struct node *head) {
   while(head != NULL) {
      struct node *next = head->next;
      free(head);
      head = next;
   }
}


/////////////////// ROOMLIST //////////////////////////

struct room* insertR(struct room *head, char *name) {
   if(findR(head, name) != NULL) {
      printf("Duplicate room: %s\n", name);
      return head;
   }
   struct room *r = (struct room*) malloc(sizeof(struct room));
   strncpy(r->name, name, NAMELEN - 1);
   r->name[NAMELEN - 1] = '\0';
   r->members = NULL;
   r->next = head;
   return r;
}

struct room* findR(struct room *head, char *name) {
   struct room *current = head;
   while(current != NULL) {
      if(strcmp(current->name, name) == 0)
         return current;
      current = current->next;
   }
   return NULL;
}

bool isMember(struct room *r, int socket) {
   struct member *m = r->members;
   while(m != NULL) {
      if(m->socket == socket)
         return true;
      m = m->next;
   }
   return false;
}

int addMember(struct room *r, int socket) {
   if(isMember(r, socket))
      return -1;
   struct member *m = (struct member*) malloc(sizeof(struct member));
   m->socket = socket;
   m->next = r->members;
   r->members = m;
   return 0;
}

int removeMember(struct room *r, int socket) {
   struct member *current = r->members, *prev = NULL;
   while(current != NULL) {
      if(current->socket == socket) {
         if(prev == NULL) r->members = current->next;
         else prev->next = current->next;
         free(current);
         return 0;
      }
      prev = current;
      current = current->next;
   }
   return -1;
}

void removeFromAllRooms(struct room *head, int socket) {
   struct room *r = head;
   while(r != NULL) {
      removeMember(r, socket);
      r = r->next;
   }
}

bool shareRoom(struct room *head, int a, int b) {
   struct room *r = head;
   while(r != NULL) {
      if(isMember(r, a) && isMember(r, b))
         return true;
      r = r->next;
   }
   return false;
}

void freeR(struct room *head) {
   while(head != NULL) {
      struct room *next = head->next;
      struct member *m = head->members;
      while(m != NULL) {
         struct member *mnext = m->next;
         free(m);
         m = mnext;
      }
      free(head);
      head = next;
   }
}


/////////////////// DIRECT CONNECTIONS (DM) ///////////

bool isDM(struct dm *head, int a, int b) {
   struct dm *d = head;
   while(d != NULL) {
      if((d->sock_a == a && d->sock_b == b) ||
         (d->sock_a == b && d->sock_b == a))
         return true;
      d = d->next;
   }
   return false;
}

struct dm* insertDM(struct dm *head, int a, int b) {
   if(isDM(head, a, b))
      return head;
   struct dm *d = (struct dm*) malloc(sizeof(struct dm));
   d->sock_a = a;
   d->sock_b = b;
   d->next = head;
   return d;
}

struct dm* removeDM(struct dm *head, int a, int b) {
   struct dm *current = head, *prev = NULL;
   while(current != NULL) {
      if((current->sock_a == a && current->sock_b == b) ||
         (current->sock_a == b && current->sock_b == a)) {
         if(prev == NULL) head = current->next;
         else prev->next = current->next;
         free(current);
         return head;
      }
      prev = current;
      current = current->next;
   }
   return head;
}

struct dm* removeAllDM(struct dm *head, int socket) {
   struct dm *current = head, *prev = NULL;
   while(current != NULL) {
      struct dm *next = current->next;
      if(current->sock_a == socket || current->sock_b == socket) {
         if(prev == NULL) head = next;
         else prev->next = next;
         free(current);
      } else {
         prev = current;
      }
      current = next;
   }
   return head;
}

void freeDM(struct dm *head) {
   while(head != NULL) {
      struct dm *next = head->next;
      free(head);
      head = next;
   }
}