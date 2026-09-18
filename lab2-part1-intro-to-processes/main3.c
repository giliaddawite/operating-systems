/*
 * main3.c
 *
 * Process Lab: forks NUM_CHILDREN child processes. Each child loops a
 * random number of times (1-30), sleeping a random amount (0-10 sec)
 * on every iteration, printing its own pid and its parent's pid. The
 * parent waits for every child to finish and reports each completion
 * as it happens.
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/wait.h>
#include  <unistd.h>
#include  <time.h>

#define  NUM_CHILDREN  2

void  ChildProcess(void);                 /* one child's sleep/report loop     */
void  ParentProcess(int num_children);    /* wait for every child to complete  */

int  main(void)
{
     pid_t  pid;
     int    i;

     /* Fork NUM_CHILDREN children in a loop instead of one fork() call
        per child -- scales to any number of children just by changing
        NUM_CHILDREN above. */
     for (i = 0; i < NUM_CHILDREN; i++) {
          pid = fork();
          if (pid == 0)
               ChildProcess();       /* never returns -- exits internally */
          /* pid > 0: still the original parent -- loop around and fork
             the next child */
     }

     /* Only the original parent reaches this line: every child calls
        exit() from inside ChildProcess() and never falls out of the
        loop above. */
     ParentProcess(NUM_CHILDREN);

     return 0;
}

/*
 * ChildProcess
 *
 * Runs in a single child. Loops a random number of times (up to 30),
 * printing a "going to sleep" message, sleeping a random amount (up
 * to 10 sec), then printing a "woke up" message with its parent's
 * pid. Seeds the RNG with its own pid mixed in so sibling children
 * don't produce identical random sequences.
 */
void  ChildProcess(void)
{
     pid_t  pid, ppid;
     int    i, count, sleep_time;

     pid  = getpid();
     ppid = getppid();

     srandom((unsigned int) (time(NULL) ^ pid));

     count = (random() % 30) + 1;                /* 1 to 30 iterations */

     for (i = 0; i < count; i++) {
          printf("Child Pid: %d is going to sleep!\n", pid);
          fflush(stdout);

          sleep_time = random() % 11;             /* 0 to 10 seconds */
          sleep(sleep_time);

          printf("Child Pid: %d is awake!\nWhere is my Parent: %d?\n", pid, ppid);
          fflush(stdout);
     }

     exit(0);
}

/*
 * ParentProcess
 *
 * Runs in the original parent only. Blocks on wait() once per child,
 * printing a completion message with that child's pid each time one
 * exits, until all num_children have been reaped.
 */
void  ParentProcess(int num_children)
{
     int    status;
     pid_t  done_pid;
     int    remaining = num_children;

     while (remaining > 0) {
          done_pid = wait(&status);
          if (done_pid > 0) {
               printf("Child Pid: %d has completed\n", done_pid);
               fflush(stdout);
               remaining--;
          }
     }
}