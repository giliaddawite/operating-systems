#include <errno.h>
#include <limits.h>
#include <netinet/in.h>   // structure for storing address information
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>   // for socket APIs
#include <sys/types.h>
#include <unistd.h>
#include "list/list.h"

#define PORT 9001
#define ACK "ACK"
#define BUF_SIZE 1024
#define DELIM " "

// Globals so cleanup() can reach them from anywhere
static list_t *mylist = NULL;
static int servSockD = -1;
static int clientSocket = -1;
static volatile sig_atomic_t running = 1;

// Ctrl-C only flips a flag; the real cleanup happens in main
static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

// Free the list and close both sockets (safe to call more than once)
static void cleanup(void)
{
    if (mylist != NULL) {
        list_free(mylist);
        mylist = NULL;
    }
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
    }
    if (servSockD >= 0) {
        close(servSockD);
        servSockD = -1;
    }
}

// Read the next token as an int. Returns 1 on success, 0 if the token is
// missing, not a number, or out of int range.
static int next_int(int *out)
{
    char *tok = strtok(NULL, DELIM);
    char *end = NULL;
    long v;

    if (tok == NULL)
        return 0;

    errno = 0;
    v = strtol(tok, &end, 10);
    if (end == tok || *end != '\0' || errno == ERANGE || v > INT_MAX || v < INT_MIN)
        return 0;

    *out = (int)v;
    return 1;
}

// Run one command. The reply is written into out.
// Returns 1 if the client asked to exit, 0 otherwise.
static int handle_command(char *line, char *out, size_t outsz)
{
    int idx, val, len;
    char *token = strtok(line, DELIM);

    out[0] = '\0';

    if (token == NULL) {
        snprintf(out, outsz, "ERROR: empty command");
        return 0;
    }

    len = list_length(mylist);

    if (strcmp(token, "exit") == 0) {
        return 1;
    }
    else if (strcmp(token, "get_length") == 0) {
        snprintf(out, outsz, "Length = %d", len);
    }
    else if (strcmp(token, "print") == 0) {
        char *str = listToString(mylist);
        if (str == NULL) {
            snprintf(out, outsz, "ERROR: out of memory");
        } else {
            snprintf(out, outsz, "%s", str);
            free(str);                       // listToString mallocs
        }
    }
    else if (strcmp(token, "add_front") == 0) {
        if (!next_int(&val)) {
            snprintf(out, outsz, "ERROR: usage: add_front <value>");
        } else {
            list_add_to_front(mylist, val);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "add_back") == 0) {
        if (!next_int(&val)) {
            snprintf(out, outsz, "ERROR: usage: add_back <value>");
        } else {
            list_add_to_back(mylist, val);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "add_position") == 0) {
        if (!next_int(&idx) || !next_int(&val)) {
            snprintf(out, outsz, "ERROR: usage: add_position <index> <value>");
        } else if (idx < 1 || idx > len + 1) {
            snprintf(out, outsz, "ERROR: index out of range (valid: 1 to %d)", len + 1);
        } else {
            list_add_at_index(mylist, val, idx);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "remove_back") == 0) {
        if (len == 0) {
            snprintf(out, outsz, "ERROR: list is empty");
        } else {
            val = list_remove_from_back(mylist);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "remove_front") == 0) {
        if (len == 0) {
            snprintf(out, outsz, "ERROR: list is empty");
        } else {
            val = list_remove_from_front(mylist);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "remove_position") == 0) {
        if (!next_int(&idx)) {
            snprintf(out, outsz, "ERROR: usage: remove_position <index>");
        } else if (idx < 1 || idx > len) {
            snprintf(out, outsz, "ERROR: index out of range (list has %d items)", len);
        } else {
            val = list_remove_at_index(mylist, idx);
            snprintf(out, outsz, "%s%d", ACK, val);
        }
    }
    else if (strcmp(token, "get") == 0) {
        if (!next_int(&idx)) {
            snprintf(out, outsz, "ERROR: usage: get <index>");
        } else if (idx < 1 || idx > len) {
            snprintf(out, outsz, "ERROR: index out of range (list has %d items)", len);
        } else {
            val = list_get_elem_at(mylist, idx);
            snprintf(out, outsz, "VALUE = %d", val);
        }
    }
    else {
        // covers "menu" and typos so the client never gets stale data
        snprintf(out, outsz, "ERROR: unknown command");
    }

    return 0;
}

int main(void)
{
    struct sockaddr_in servAddr;
    struct sigaction sa;
    char buf[BUF_SIZE];
    char sbuf[BUF_SIZE];
    ssize_t n;
    int opt = 1;

    // SIGINT handler without SA_RESTART, so a blocked accept()/recv()
    // returns and the loop can notice the flag
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);   // a vanished client shouldn't kill the server

    servSockD = socket(AF_INET, SOCK_STREAM, 0);
    if (servSockD < 0) {
        perror("socket");
        return 1;
    }

    // lets you restart right away without "address already in use"
    if (setsockopt(servSockD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror("setsockopt");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORT);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(servSockD, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("bind");
        cleanup();
        return 1;
    }
    if (listen(servSockD, 1) < 0) {
        perror("listen");
        cleanup();
        return 1;
    }

    mylist = list_alloc();
    if (mylist == NULL) {
        fprintf(stderr, "list_alloc failed\n");
        cleanup();
        return 1;
    }

    clientSocket = accept(servSockD, NULL, NULL);
    if (clientSocket < 0) {
        if (errno != EINTR)
            perror("accept");
        cleanup();
        return running ? 1 : 0;   // Ctrl-C while waiting is a normal exit
    }

    while (running) {
        n = recv(clientSocket, buf, sizeof(buf) - 1, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;         // interrupted by Ctrl-C, loop re-checks the flag
            perror("recv");
            break;
        }
        if (n == 0)
            break;                // client disconnected

        buf[n] = '\0';

        if (handle_command(buf, sbuf, sizeof(sbuf)))
            break;                // "exit"

        // send only the real message plus its '\0'
        if (send(clientSocket, sbuf, strlen(sbuf) + 1, 0) < 0) {
            perror("send");
            break;
        }
    }

    cleanup();
    return 0;
}