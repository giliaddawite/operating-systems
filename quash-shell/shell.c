#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;
 
// PIDs of the current foreground job (a pipeline can have several processes).
// The SIGALRM handler reads these; volatile because a signal handler touches them.
volatile pid_t fg_pids[MAX_STAGES];
volatile int fg_count = 0;
 
// One command in a pipeline: its arguments plus optional redirect files.
typedef struct {
    char *argv[MAX_COMMAND_LINE_ARGS];
    char *infile;     // from "< file"  (NULL if none)
    char *outfile;    // from "> file"  (NULL if none)
} Command;
 
/* ------------------------------------------------------------------ */
/* Signal handlers                                                     */
/* ------------------------------------------------------------------ */
 
/*
 * SIGINT (ctrl-C) handler for the shell itself. Just prints a newline so the
 * next prompt starts on a fresh line. write() is used because it's safe inside
 * a signal handler (printf isn't).
 */
void handle_sigint(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
}
 
/*
 * SIGALRM handler: the foreground job's time is up, so kill every process in
 * it. SIGKILL can't be caught or ignored. Background jobs never go in
 * fg_pids, so they're never touched.
 */
void handle_sigalrm(int sig) {
    (void)sig;
    for (int i = 0; i < fg_count; i++) {
        kill(fg_pids[i], SIGKILL);
    }
}
 
/* ------------------------------------------------------------------ */
/* Tokenizing                                                          */
/* ------------------------------------------------------------------ */
 
/*
 * Split a line into tokens on whitespace. Handles 'single' and "double"
 * quotes (quotes are removed, spaces inside them are kept) and replaces any
 * token starting with '$' with that environment variable's value (empty
 * string if unset). Edits the line in place. Returns the token count;
 * arguments[] is NULL-terminated.
 */
int tokenize(char *line, char *arguments[]) {
    int n = 0;
    char *p = line;
 
    while (*p != '\0' && n < MAX_COMMAND_LINE_ARGS - 1) {
        while (*p != '\0' && strchr(delimiters, *p) != NULL) p++;   // skip spaces
        if (*p == '\0') break;
 
        char *start = p;     // where this token begins
        char *w = p;         // where we write the cleaned-up token
        char quote = 0;      // current open quote character, or 0
 
        while (*p != '\0') {
            if (quote) {
                if (*p == quote) { quote = 0; p++; }
                else             { *w++ = *p++; }
            } else if (*p == '"' || *p == '\'') {
                quote = *p++;
            } else if (strchr(delimiters, *p) != NULL) {
                break;
            } else {
                *w++ = *p++;
            }
        }
 
        // Remember whether we stopped at a delimiter BEFORE writing the '\0'
        // (when no quotes were removed, w == p and that write lands on it).
        bool at_delimiter = (*p != '\0');
        *w = '\0';
        if (at_delimiter) p++;           // step past the delimiter
 
        if (start[0] == '$') {
            char *val = getenv(start + 1);
            arguments[n++] = val ? val : "";
        } else {
            arguments[n++] = start;
        }
    }
    arguments[n] = NULL;
    return n;
}
 
/* True for the three shell operators */
bool is_operator(const char *t) {
    return strcmp(t, "|") == 0 || strcmp(t, "<") == 0 || strcmp(t, ">") == 0;
}
 
/*
 * Turn the flat token list into a pipeline: split on "|", and pull "< file"
 * and "> file" out of each command into infile/outfile. Returns false (after
 * printing a message) on a syntax error such as "ls |" or "cat >".
 */
bool parse_pipeline(int argc, char *arguments[], Command cmds[], int *ncmds) {
    int n = 0;     // index of the command we're filling in
    int a = 0;     // how many args that command has so far
    memset(&cmds[0], 0, sizeof(Command));
 
    for (int i = 0; i < argc; i++) {
        char *t = arguments[i];
 
        if (strcmp(t, "|") == 0) {
            if (a == 0) {
                fprintf(stderr, "syntax error: missing command before '|'\n");
                return false;
            }
            cmds[n].argv[a] = NULL;
            if (++n >= MAX_STAGES) {
                fprintf(stderr, "too many commands in one pipeline\n");
                return false;
            }
            memset(&cmds[n], 0, sizeof(Command));
            a = 0;
        } else if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0) {
            if (i + 1 >= argc || is_operator(arguments[i + 1])) {
                fprintf(stderr, "syntax error: missing file after '%s'\n", t);
                return false;
            }
            if (t[0] == '<') cmds[n].infile  = arguments[++i];
            else             cmds[n].outfile = arguments[++i];
        } else {
            cmds[n].argv[a++] = t;
        }
    }
 
    if (a == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return false;
    }
    cmds[n].argv[a] = NULL;
    *ncmds = n + 1;
    return true;
}
 
/*
 * If the line ends in '&' (like "a.out &" or "a.out&"), chop it off the
 * string and return true. Works on the raw line, before tokenizing.
 */
bool strip_background(char *line) {
    size_t len = strlen(line);
    while (len > 0 && strchr(delimiters, line[len - 1]) != NULL) {
        len--;                       // skip trailing whitespace
    }
    if (len > 0 && line[len - 1] == '&') {
        line[len - 1] = '\0';
        return true;
    }
    return false;
}
 
/* ------------------------------------------------------------------ */
/* Built-in commands                                                   */
/* ------------------------------------------------------------------ */
 
void builtin_cd(int argc, char *arguments[]) {
    const char *target = (argc > 1) ? arguments[1] : getenv("HOME");
    if (target == NULL) {
        fprintf(stderr, "cd: no directory given and HOME is not set\n");
        return;
    }
    if (chdir(target) != 0) {
        perror("cd failed");
    }
}
 
void builtin_pwd(void) {
    char cwd[MAX_COMMAND_LINE_LEN];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() failed");
    } else {
        printf("%s\n", cwd);
    }
}
 
void builtin_echo(int argc, char *arguments[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s%s", arguments[i], (i < argc - 1) ? " " : "");
    }
    printf("\n");
}
 
/* "env" prints everything, "env NAME" prints just that variable's value */
void builtin_env(int argc, char *arguments[]) {
    if (argc > 1) {
        char *val = getenv(arguments[1]);
        if (val) printf("%s\n", val);
        return;
    }
    for (char **e = environ; *e != NULL; e++) {
        printf("%s\n", *e);
    }
}
 
/* Accepts both "setenv NAME=value" and "setenv NAME value" */
void builtin_setenv(int argc, char *arguments[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: setenv NAME=value  (or: setenv NAME value)\n");
        return;
    }
    char *eq = strchr(arguments[1], '=');
    int rc;
    if (eq != NULL) {
        *eq = '\0';
        rc = setenv(arguments[1], eq + 1, 1);
    } else if (argc > 2) {
        rc = setenv(arguments[1], arguments[2], 1);
    } else {
        rc = setenv(arguments[1], "", 1);
    }
    if (rc != 0) perror("setenv failed");
}
 
bool is_builtin(const char *name) {
    return strcmp(name, "exit")   == 0 || strcmp(name, "cd")  == 0 ||
           strcmp(name, "pwd")    == 0 || strcmp(name, "echo") == 0 ||
           strcmp(name, "env")    == 0 || strcmp(name, "setenv") == 0;
}
 
int count_args(char *argv[]) {
    int n = 0;
    while (argv[n] != NULL) n++;
    return n;
}
 
void run_builtin(int argc, char *arguments[]) {
    // ("exit" is handled in main when it's alone; inside a pipeline it just ends that stage)
    if      (strcmp(arguments[0], "exit")   == 0) return;
    else if (strcmp(arguments[0], "cd")     == 0) builtin_cd(argc, arguments);
    else if (strcmp(arguments[0], "pwd")    == 0) builtin_pwd();
    else if (strcmp(arguments[0], "echo")   == 0) builtin_echo(argc, arguments);
    else if (strcmp(arguments[0], "env")    == 0) builtin_env(argc, arguments);
    else if (strcmp(arguments[0], "setenv") == 0) builtin_setenv(argc, arguments);
}
 
/* ------------------------------------------------------------------ */
/* Redirection and running commands                                    */
/* ------------------------------------------------------------------ */
 
/*
 * Hook up a command's "< file" and "> file" by opening the file and using
 * dup2 to make it stdin/stdout. Returns false (after perror) if a file
 * can't be opened.
 */
bool apply_redirects(Command *c) {
    if (c->infile != NULL) {
        int fd = open(c->infile, O_RDONLY);
        if (fd < 0) {
            perror(c->infile);
            return false;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->outfile != NULL) {
        int fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(c->outfile);
            return false;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return true;
}
 
/*
 * A lone built-in (like "echo hi > file") runs inside the shell itself, since
 * things like cd and setenv have to change the shell's own state. To
 * redirect it we point stdin/stdout at the files, run it, then put them back.
 */
void run_builtin_redirected(Command *c) {
    fflush(stdout);
    int saved_in  = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);
 
    if (apply_redirects(c)) {
        run_builtin(count_args(c->argv), c->argv);
    }
 
    fflush(stdout);              // push output into the file before switching back
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_in);
    close(saved_out);
}
 
/*
 * Run a pipeline of n commands (n = 1 is just a normal command). Every
 * command gets its own child process, and a pipe connects each command's
 * stdout to the next one's stdin. The parent waits for all of them unless
 * it's a background job.
 */
void run_pipeline(Command cmds[], int n, bool background) {
    int pipes[MAX_STAGES][2];
    pid_t pids[MAX_STAGES];
    int started = 0;
 
    // Flush first so buffered output isn't copied into the children and printed twice
    fflush(stdout);
    fflush(stderr);
 
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe() failed");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
    }
 
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork() failed");
            break;
        }
 
        if (pid == 0) {
            // Child: ctrl-C should kill the program, so undo the shell's handler.
            signal(SIGINT, SIG_DFL);
 
            // Background jobs move to their own process group so a ctrl-C meant
            // for the foreground doesn't also hit them.
            if (background) {
                setpgid(0, 0);
            }
 
            // Connect to the neighbors in the pipeline...
            if (i > 0)     dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1], STDOUT_FILENO);
 
            // ...then close every pipe end. If any stay open, readers never see EOF.
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
 
            // Explicit < and > override the pipe (same as bash)
            // (Children leave with _exit, not exit: exit() would "rewind" a shared
            // stdin file and make the parent shell re-read lines.)
            if (!apply_redirects(&cmds[i])) {
                _exit(EXIT_FAILURE);
            }
 
            // Built-ins in a pipeline run in the child (e.g. "env | grep PATH")
            if (is_builtin(cmds[i].argv[0])) {
                run_builtin(count_args(cmds[i].argv), cmds[i].argv);
                fflush(stdout);          // _exit won't flush for us
                _exit(0);
            }
 
            // Swap this process over to the new program.
            // execvp only returns if something went wrong.
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp() failed");
            fprintf(stderr, "An error occurred.\n");
            _exit(EXIT_FAILURE);
        }
 
        pids[started++] = pid;
        if (!background) {
            fg_pids[started - 1] = pid;
            fg_count = started;
        }
    }
 
    // Parent: it doesn't use the pipes itself, so close both ends of all of them
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
 
    // Background jobs get a fresh prompt right away, no waiting.
    // (Finished ones are cleaned up by reap_background() in main's loop.)
    if (background) {
        return;
    }
 
    // Foreground job: start the countdown, then wait for every child
    alarm(FG_TIMEOUT_SECS);
 
    for (int i = 0; i < started; i++) {
        int status;
        while (waitpid(pids[i], &status, 0) < 0) {
            if (errno == EINTR) {
                continue;            // a signal interrupted the wait; keep waiting
            }
            perror("waitpid() failed");
            break;
        }
    }
 
    // All children gone: forget them and cancel the countdown
    fg_count = 0;
    alarm(0);
}
 
/*
 * Collect any background children that have already finished so they don't
 * pile up as zombies. WNOHANG means "don't block if nobody's done yet".
 */
void reap_background(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // keep reaping until there's nothing left to collect
    }
}
 
/* ------------------------------------------------------------------ */
 
int main() {
    // Stores the string typed into the command line.
    char command_line[MAX_COMMAND_LINE_LEN];
    char cmd_bak[MAX_COMMAND_LINE_LEN];
 
    // Stores the tokenized command line input.
    char *arguments[MAX_COMMAND_LINE_ARGS];
 
    // The parsed pipeline (one Command per "|"-separated piece)
    Command cmds[MAX_STAGES];
 
    // Catch ctrl-C in the shell. No SA_RESTART, so a ctrl-C at the prompt
    // interrupts fgets and we can print a fresh prompt.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
 
    // Catch the timer's SIGALRM (Task 5)
    struct sigaction sa_alarm;
    memset(&sa_alarm, 0, sizeof(sa_alarm));
    sa_alarm.sa_handler = handle_sigalrm;
    sigemptyset(&sa_alarm.sa_mask);
    sa_alarm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alarm, NULL);
 
    while (true) {
 
        reap_background();   // clean up any finished background jobs
 
        do {
            // Print the prompt: current working directory followed by "> "
            char cwd[MAX_COMMAND_LINE_LEN];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                perror("getcwd() failed");
                strcpy(cwd, "?");
            }
            printf("%s%s", cwd, prompt);
            fflush(stdout);
 
            // Read input from stdin and store it in command_line. If there's an
            // error, exit immediately.
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
                if (errno == EINTR) {
                    // ctrl-C at the prompt: not a real error, just re-prompt
                    clearerr(stdin);
                    command_line[0] = 0x0A;
                    continue;
                }
                fprintf(stderr, "fgets error");
                exit(0);
            }
 
        } while (command_line[0] == 0x0A && !feof(stdin));  // while just ENTER pressed
        command_line[strlen(command_line) - 1] = '\0';
 
        // If the user input was EOF (ctrl+d), exit the shell.
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }
 
        // Tokenize a copy, so command_line stays intact
        strcpy(cmd_bak, command_line);
        bool background = strip_background(cmd_bak);   // removes a trailing '&'
        int argc = tokenize(cmd_bak, arguments);
        if (argc == 0) continue;  // line was only whitespace
 
        // Split into pipeline stages and pull out < and > redirects
        int ncmds = 0;
        if (!parse_pipeline(argc, arguments, cmds, &ncmds)) continue;
 
        if (ncmds == 1 && strcmp(cmds[0].argv[0], "exit") == 0) {
            break;
        } else if (ncmds == 1 && is_builtin(cmds[0].argv[0])) {
            run_builtin_redirected(&cmds[0]);
        } else {
            run_pipeline(cmds, ncmds, background);
        }
    }
    return 0;
}