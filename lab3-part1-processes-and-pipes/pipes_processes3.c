#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/**
 * Executes the command "cat scores | grep <argument> | sort" using three
 * processes:
 *   P1 (parent)        -> cat scores
 *   P2 (child)         -> grep <argument>
 *   P3 (child's child) -> sort
 *
 * Usage: ./pipes_processes3 <grep argument>
 *
 * Each process execs its command directly (as in pipes_processes2.c), so
 * no process waits on another; the shell prompt may reappear before sort
 * finishes printing.
 */

#define READ_END  0
#define WRITE_END 1

int main(int argc, char **argv)
{
  int pipefd1[2];   // cat  -> grep
  int pipefd2[2];   // grep -> sort
  pid_t pid1, pid2;

  if (argc != 2)
    {
      fprintf(stderr, "Usage: %s <grep argument>\n", argv[0]);
      return 1;
    }

  char *cat_args[]  = {"cat", "scores", NULL};
  char *grep_args[] = {"grep", argv[1], NULL};
  char *sort_args[] = {"sort", NULL};

  // first pipe: connects cat's output to grep's input

  if (pipe(pipefd1) == -1)
    {
      perror("pipe");
      return 1;
    }

  pid1 = fork();

  if (pid1 < 0)
    {
      perror("fork");
      return 1;
    }

  if (pid1 == 0)
    {
      // P2 gets here and will handle "grep <argument>", but first it
      // creates the second pipe and forks P3 to handle "sort"

      if (pipe(pipefd2) == -1)
        {
          perror("pipe");
          exit(1);
        }

      pid2 = fork();

      if (pid2 < 0)
        {
          perror("fork");
          exit(1);
        }

      if (pid2 == 0)
        {
          // P3 (child's child) handles "sort"

          // replace standard input with input part of second pipe

          dup2(pipefd2[READ_END], STDIN_FILENO);

          // close every pipe end this process doesn't use, including
          // both ends of the first pipe inherited from P2

          close(pipefd2[READ_END]);
          close(pipefd2[WRITE_END]);
          close(pipefd1[READ_END]);
          close(pipefd1[WRITE_END]);

          // execute sort

          execvp("sort", sort_args);
          perror("execvp sort");
          exit(1);
        }
      else
        {
          // P2 (child) handles "grep <argument>"

          // replace standard input with input part of first pipe
          // and standard output with output part of second pipe

          dup2(pipefd1[READ_END], STDIN_FILENO);
          dup2(pipefd2[WRITE_END], STDOUT_FILENO);

          // close all original pipe ends (stdin/stdout now hold them)

          close(pipefd1[READ_END]);
          close(pipefd1[WRITE_END]);
          close(pipefd2[READ_END]);
          close(pipefd2[WRITE_END]);

          // execute grep

          execvp("grep", grep_args);
          perror("execvp grep");
          exit(1);
        }
    }
  else
    {
      // P1 (parent) handles "cat scores"

      // replace standard output with output part of first pipe

      dup2(pipefd1[WRITE_END], STDOUT_FILENO);

      // close both original ends (stdout now holds the write end)

      close(pipefd1[READ_END]);
      close(pipefd1[WRITE_END]);

      // execute cat

      execvp("cat", cat_args);
      perror("execvp cat");
      return 1;
    }
}