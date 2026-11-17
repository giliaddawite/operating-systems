#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define NUM_THREADS 10   // total worker threads used to divide the work
#define DEFAULT_SIZE 20  // matrix size if none is given on the command line

int MAX;  // matrix dimension (MAX x MAX), set in main from the command line

// MAX is only known at run time, so the matrices are allocated on the heap
// in main (as one MAX*MAX block each) instead of being fixed-size globals.
int *matA;
int *matB;

int *matSumResult;
int *matDiffResult;
int *matProductResult;

void fillMatrix(int matrix[MAX][MAX]) {
    for(int i = 0; i<MAX; i++) {
        for(int j = 0; j<MAX; j++) {
            matrix[i][j] = rand()%10+1;
        }
    }
}

void printMatrix(int matrix[MAX][MAX]) {
    for(int i = 0; i<MAX; i++) {
        for(int j = 0; j<MAX; j++) {
            printf("%5d", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Each thread owns a contiguous band of rows. Thread i handles rows
// [i*MAX/NUM_THREADS, (i+1)*MAX/NUM_THREADS). For a 20x20 matrix and
// 10 threads that is 2 rows per thread. No two threads touch the same
// result cell, so no mutex is needed.
static void rowRange(int threadNum, int *start, int *end) {
    *start = threadNum * MAX / NUM_THREADS;
    *end   = (threadNum + 1) * MAX / NUM_THREADS;
}

// Fetches the thread number from the argument, and sets every cell of
// matSumResult in this thread's rows to the sum of the values at the
// same coordinates of matA and matB.
void* computeSum(void* args) { // pass in the number of the ith thread
    int (*A)[MAX] = (int (*)[MAX])matA;
    int (*B)[MAX] = (int (*)[MAX])matB;
    int (*R)[MAX] = (int (*)[MAX])matSumResult;
    int start, end;
    rowRange(*(int*)args, &start, &end);

    for(int i = start; i<end; i++) {
        for(int j = 0; j<MAX; j++) {
            R[i][j] = A[i][j] + B[i][j];
        }
    }
    return NULL;
}

// Fetches the thread number from the argument, and sets every cell of
// matDiffResult in this thread's rows to the difference of the values
// at the same coordinates of matA and matB.
void* computeDiff(void* args) { // pass in the number of the ith thread
    int (*A)[MAX] = (int (*)[MAX])matA;
    int (*B)[MAX] = (int (*)[MAX])matB;
    int (*R)[MAX] = (int (*)[MAX])matDiffResult;
    int start, end;
    rowRange(*(int*)args, &start, &end);

    for(int i = start; i<end; i++) {
        for(int j = 0; j<MAX; j++) {
            R[i][j] = A[i][j] - B[i][j];
        }
    }
    return NULL;
}

// Fetches the thread number from the argument, and sets every cell of
// matProductResult in this thread's rows to the inner (dot) product of
// the matching row of matA and column of matB.
void* computeProduct(void* args) { // pass in the number of the ith thread
    int (*A)[MAX] = (int (*)[MAX])matA;
    int (*B)[MAX] = (int (*)[MAX])matB;
    int (*R)[MAX] = (int (*)[MAX])matProductResult;
    int start, end;
    rowRange(*(int*)args, &start, &end);

    for(int i = start; i<end; i++) {
        for(int j = 0; j<MAX; j++) {
            int sum = 0;
            for(int k = 0; k<MAX; k++) {
                sum += A[i][k] * B[k][j];
            }
            R[i][j] = sum;
        }
    }
    return NULL;
}

// Entry point for each of the 10 threads: does the add, subtract and
// dot product for this thread's rows, then frees the malloc'd thread number.
void* worker(void* args) {
    computeSum(args);
    computeDiff(args);
    computeProduct(args);
    free(args);
    return NULL;
}

// Spawn NUM_THREADS (10) threads; each computes its share of all three
// result matrices.
int main(int argc, char *argv[]) {
    srand(time(0));  // Do Not Remove. Just ignore and continue below.

    // 0. Get the matrix size from the command line and assign it to MAX
    MAX = DEFAULT_SIZE;
    if (argc > 1) {
        MAX = atoi(argv[1]);
    }
    if (MAX <= 0) {
        fprintf(stderr, "Usage: %s [matrix size > 0]\n", argv[0]);
        return 1;
    }

    matA             = malloc(sizeof(int) * MAX * MAX);
    matB             = malloc(sizeof(int) * MAX * MAX);
    matSumResult     = malloc(sizeof(int) * MAX * MAX);
    matDiffResult    = malloc(sizeof(int) * MAX * MAX);
    matProductResult = malloc(sizeof(int) * MAX * MAX);
    if (!matA || !matB || !matSumResult || !matDiffResult || !matProductResult) {
        perror("malloc");
        return 1;
    }

    // 1. Fill the matrices (matA and matB) with random values.
    fillMatrix((int (*)[MAX])matA);
    fillMatrix((int (*)[MAX])matB);

    // 2. Print the initial matrices.
    printf("Matrix A:\n");
    printMatrix((int (*)[MAX])matA);
    printf("Matrix B:\n");
    printMatrix((int (*)[MAX])matB);

    // 3. Create pthread_t objects for our threads.
    pthread_t threads[NUM_THREADS];

    // 4. Create the threads. Each one gets its own malloc'd copy of its
    //    thread number, which it uses to work out which rows it owns and
    //    frees when it is done.
    for(int i = 0; i<NUM_THREADS; i++) {
        int *threadNum = malloc(sizeof(int));
        if (threadNum == NULL) {
            perror("malloc");
            return 1;
        }
        *threadNum = i;
        if (pthread_create(&threads[i], NULL, worker, threadNum) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    // 5. Wait for all threads to finish.
    for(int i = 0; i<NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 6. Print the results.
    printf("Results:\n");
    printf("Sum:\n");
    printMatrix((int (*)[MAX])matSumResult);
    printf("Difference:\n");
    printMatrix((int (*)[MAX])matDiffResult);
    printf("Product:\n");
    printMatrix((int (*)[MAX])matProductResult);

    free(matA);
    free(matB);
    free(matSumResult);
    free(matDiffResult);
    free(matProductResult);
    return 0;
}