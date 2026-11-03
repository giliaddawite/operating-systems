#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define DAD_MIN  0
#define DAD_MAX  100
#define NEED_MIN 0
#define NEED_MAX 50
#define MOM_MIN  0
#define MOM_MAX  125

void dad(int *BankAccount, sem_t *mutex)
{
    int localBalance = 0;
    srand(time(NULL) ^ getpid());

    while (1) {
        sleep(rand() % 6);   // 0-5 seconds
        printf("Dear old Dad: Attempting to Check Balance\n");

        if (rand() % 2 == 0) {
            if (localBalance < 100) {
                sem_wait(mutex);
                localBalance = *BankAccount;
                int amount = DAD_MIN + rand() % (DAD_MAX - DAD_MIN + 1);

                if (rand() % 2 == 0) {
                    localBalance += amount;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                } else {
                    printf("Dear old Dad: Doesn't have any money to give\n");
                }

                *BankAccount = localBalance;
                sem_post(mutex);
            } else {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            }
        } else {
            printf("Dear old Dad: Last Checking Balance = $%d\n", localBalance);
        }
    }
}

void mom(int *BankAccount, sem_t *mutex)
{
    int localBalance = 0;
    srand(time(NULL) ^ getpid());

    while (1) {
        sleep(rand() % 11);   // 0-10 seconds
        printf("Loveable Mom: Attempting to Check Balance\n");

        if (localBalance <= 100) {
            sem_wait(mutex);
            localBalance = *BankAccount;
            int amount = MOM_MIN + rand() % (MOM_MAX - MOM_MIN + 1);

            localBalance += amount;
            printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);

            *BankAccount = localBalance;
            sem_post(mutex);
        }
    }
}

void student(int id, int *BankAccount, sem_t *mutex)
{
    int localBalance = 0;
    srand(time(NULL) ^ getpid());

    while (1) {
        sleep(rand() % 6);   // 0-5 seconds
        printf("Poor Student #%d: Attempting to Check Balance\n", id);

        if (rand() % 2 == 0) {
            sem_wait(mutex);
            localBalance = *BankAccount;
            int need = NEED_MIN + rand() % (NEED_MAX - NEED_MIN + 1);
            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student #%d: Withdraws $%d / Balance = $%d\n", id, need, localBalance);
            } else {
                printf("Poor Student #%d: Not Enough Cash ($%d)\n", id, localBalance);
            }

            *BankAccount = localBalance;
            sem_post(mutex);
        } else {
            printf("Poor Student #%d: Last Checking Balance = $%d\n", id, localBalance);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage:  %s <parents> <children> (where parent = [1 | 2], children = [N > 0]\n", argv[0]);
        exit(1);
    }
    int parents  = atoi(argv[1]);
    int children = atoi(argv[2]);
    if ((parents != 1 && parents != 2) || children < 1) {
        printf("usage:  %s <parents> <children> (where parent = [1 | 2], children = [N > 0]\n", argv[0]);
        exit(1);
    }

    int ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) { printf("*** shmget error ***\n"); exit(1); }

    int *BankAccount = (int *) shmat(ShmID, NULL, 0);
    if (BankAccount == (int *) -1) { printf("*** shmat error ***\n"); exit(1); }
    shmctl(ShmID, IPC_RMID, NULL);   // auto-cleanup once every process dies

    *BankAccount = 0;

    sem_unlink("bankmutex");         // clear any stale semaphore from a killed run
    sem_t *mutex = sem_open("bankmutex", O_CREAT, 0644, 1);
    if (mutex == SEM_FAILED) { perror("sem_open"); exit(1); }

    // fork the Students
    for (int i = 1; i <= children; i++) {
        pid_t pid = fork();
        if (pid < 0) { printf("*** fork error ***\n"); exit(1); }
        if (pid == 0) {
            student(i, BankAccount, mutex);
            exit(0);
        }
    }

    // fork Mom if asked for
    if (parents == 2) {
        pid_t pid = fork();
        if (pid < 0) { printf("*** fork error ***\n"); exit(1); }
        if (pid == 0) {
            mom(BankAccount, mutex);
            exit(0);
        }
    }

    // the original process is Dear Old Dad
    dad(BankAccount, mutex);
    return 0;
}