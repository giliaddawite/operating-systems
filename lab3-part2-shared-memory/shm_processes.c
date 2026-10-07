#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
 
/**
 * Two processes share a bank account through System V shared memory.
 *   Parent (Dear Old Dad)  deposits money
 *   Child  (Poor Student)  withdraws money
 * Access is coordinated with Tanenbaum's strict alternation: a shared
 * Turn variable says whose go it is, and each process busy-waits until
 * Turn holds its own value.
 *
 * Shared memory layout: ShmPTR[0] = BankAccount, ShmPTR[1] = Turn
 */
 
#define BANK_ACCOUNT 0
#define TURN         1
#define NUM_SHARED   2
 
#define DAD_TURN     0
#define STUDENT_TURN 1
 
#define ITERATIONS   25
#ifndef MAX_SLEEP
#define MAX_SLEEP    5      // sleep 0..MAX_SLEEP seconds each loop
#endif
#define MAX_DEPOSIT  100    // Dad deposits 0..100
#define MAX_NEED     50     // Student needs 0..50
 
void ParentProcess(volatile int []);
void ChildProcess(volatile int []);
 
int main(void)
{
     int           ShmID;
     volatile int *ShmPTR;
     pid_t         pid;
     int           status;
 
     ShmID = shmget(IPC_PRIVATE, NUM_SHARED * sizeof(int), IPC_CREAT | 0666);
     if (ShmID < 0) {
          perror("*** shmget error (server) ***");
          exit(1);
     }
     setvbuf(stdout, NULL, _IOLBF, 0);   // line-buffer so both processes' output interleaves cleanly
     printf("Server has received a shared memory of two integers...\n");
 
     ShmPTR = (int *) shmat(ShmID, NULL, 0);
     if (ShmPTR == (void *) -1) {
          perror("*** shmat error (server) ***");
          exit(1);
     }
     printf("Server has attached the shared memory...\n");
 
     ShmPTR[BANK_ACCOUNT] = 0;
     ShmPTR[TURN]         = DAD_TURN;
     printf("Server has initialized BankAccount = %d and Turn = %d...\n",
            ShmPTR[BANK_ACCOUNT], ShmPTR[TURN]);
 
     printf("Server is about to fork a child process...\n");
     pid = fork();
     if (pid < 0) {
          perror("*** fork error (server) ***");
          exit(1);
     }
     else if (pid == 0) {
          ChildProcess(ShmPTR);
          exit(0);
     }
 
     ParentProcess(ShmPTR);
 
     wait(&status);
     printf("Server has detected the completion of its child...\n");
     shmdt((void *) ShmPTR);
     printf("Server has detached its shared memory...\n");
     shmctl(ShmID, IPC_RMID, NULL);
     printf("Server has removed its shared memory...\n");
     printf("Server exits...\n");
     exit(0);
}
 
/* Dear Old Dad: deposits money while it is his turn (Turn == 0). */
void ParentProcess(volatile int SharedMem[])
{
     int account, balance, i;
 
     srand(time(NULL) ^ getpid());
 
     for (i = 0; i < ITERATIONS; i++) {
          sleep(rand() % (MAX_SLEEP + 1));
 
          account = SharedMem[BANK_ACCOUNT];
 
          while (SharedMem[TURN] != DAD_TURN)
               ;    // no-op: wait for the Student to finish
 
          if (account <= 100) {
               balance = rand() % (MAX_DEPOSIT + 1);
               if (balance % 2 == 0) {
                    account += balance;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                           balance, account);
               }
               else {
                    printf("Dear old Dad: Doesn't have any money to give\n");
               }
          }
          else {
               printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                      account);
          }
 
          SharedMem[BANK_ACCOUNT] = account;
          SharedMem[TURN] = STUDENT_TURN;
     }
}
 
/* Poor Student: withdraws money while it is his turn (Turn == 1). */
void ChildProcess(volatile int SharedMem[])
{
     int account, balance, i;
 
     srand(time(NULL) ^ getpid());
 
     for (i = 0; i < ITERATIONS; i++) {
          sleep(rand() % (MAX_SLEEP + 1));
 
          account = SharedMem[BANK_ACCOUNT];
 
          while (SharedMem[TURN] != STUDENT_TURN)
               ;    // no-op: wait for Dad to finish
 
          balance = rand() % (MAX_NEED + 1);
          printf("Poor Student needs $%d\n", balance);
 
          if (balance <= account) {
               account -= balance;
               printf("Poor Student: Withdraws $%d / Balance = $%d\n",
                      balance, account);
          }
          else {
               printf("Poor Student: Not Enough Cash ($%d)\n", account);
          }
 
          SharedMem[BANK_ACCOUNT] = account;
          SharedMem[TURN] = DAD_TURN;
     }
}