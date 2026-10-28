// C program for implementation of Simulation 
#include<stdio.h> 
#include<limits.h>
#include<stdlib.h>
#include "process.h"
#include "util.h"


// Round robin: each process gets `quantum` ticks per turn.
// A process's wait is recorded when it finishes: finish time minus its burst.
void findWaitingTimeRR(ProcessType plist[], int n, int quantum) 
{ 
    int *rem_bt = malloc(n * sizeof(int));
    int t = 0, done = 0;

    for (int i = 0; i < n; i++) {
        rem_bt[i] = plist[i].bt;
        plist[i].wt = 0;
        if (rem_bt[i] == 0) done++;   // zero-burst guard
    }

    while (done < n) {
        int progressed = 0;

        for (int i = 0; i < n; i++) {
            if (rem_bt[i] == 0) continue;
            if (plist[i].art > t) continue;   // hasn't arrived yet, skip

            progressed = 1;
            if (rem_bt[i] > quantum) {
                t += quantum;
                rem_bt[i] -= quantum;
            } else {
                t += rem_bt[i];
                // wait = finish time - arrival - burst
                plist[i].wt = t - plist[i].art - plist[i].bt;
                rem_bt[i] = 0;
                done++;
            }
        }

        // nobody has arrived yet, so let time pass
        if (!progressed) t++;
    }
    free(rem_bt);
}

// SJF in its preemptive form (SRTF): every tick, run whichever arrived
// process has the least time left. Ties go to the lower index.
void findWaitingTimeSJF(ProcessType plist[], int n)
{ 
    int *rem_bt = malloc(n * sizeof(int));
    int t = 0, complete = 0;

    for (int i = 0; i < n; i++) {
        rem_bt[i] = plist[i].bt;
        plist[i].wt = 0;
        if (rem_bt[i] == 0) complete++;   // zero-burst guard
    }

    while (complete < n) {
        int shortest = -1;
        int min_rem = INT_MAX;

        for (int j = 0; j < n; j++) {
            if (plist[j].art <= t && rem_bt[j] > 0 && rem_bt[j] < min_rem) {
                min_rem = rem_bt[j];
                shortest = j;
            }
        }

        // nobody has arrived yet, just let time pass
        if (shortest == -1) {
            t++;
            continue;
        }

        rem_bt[shortest]--;
        t++;

        if (rem_bt[shortest] == 0) {
            complete++;
            // wt = completion time - arrival time - burst time
            plist[shortest].wt = t - plist[shortest].art - plist[shortest].bt;
        }
    }
    free(rem_bt);
}

// FCFS wait: start time minus arrival time (CPU idles until a process arrives)
void findWaitingTime(ProcessType plist[], int n)
{ 
    int t = 0;
    for (int i = 0; i < n; i++) {
        if (t < plist[i].art) t = plist[i].art;
        plist[i].wt = t - plist[i].art;
        t += plist[i].bt;
    }
} 
  
// Function to calculate turn around time 
void findTurnAroundTime( ProcessType plist[], int n)
{ 
    // calculating turnaround time by adding bt[i] + wt[i] 
    for (int  i = 0; i < n ; i++) 
        plist[i].tat = plist[i].bt + plist[i].wt; 
} 
  
// Sort by priority, highest number first. Ties keep input order (by pid).
int my_comparer(const void *this, const void *that)
{ 
    const ProcessType *a = (const ProcessType *)this;
    const ProcessType *b = (const ProcessType *)that;

    if (a->pri < b->pri) return 1;
    if (a->pri > b->pri) return -1;
    return a->pid - b->pid;
} 

//Function to calculate average time 
void findavgTimeFCFS( ProcessType plist[], int n) 
{ 
    //Function to find waiting time of all processes 
    findWaitingTime(plist, n); 
  
    //Function to find turn around time for all processes 
    findTurnAroundTime(plist, n); 
  
    //Display processes along with all details 
    printf("\n*********\nFCFS\n");
}

//Function to calculate average time 
void findavgTimeSJF( ProcessType plist[], int n) 
{ 
    //Function to find waiting time of all processes 
    findWaitingTimeSJF(plist, n); 
  
    //Function to find turn around time for all processes 
    findTurnAroundTime(plist, n); 
  
    //Display processes along with all details 
    printf("\n*********\nSJF\n");
}

//Function to calculate average time 
void findavgTimeRR( ProcessType plist[], int n, int quantum) 
{ 
    //Function to find waiting time of all processes 
    findWaitingTimeRR(plist, n, quantum); 
  
    //Function to find turn around time for all processes 
    findTurnAroundTime(plist, n); 
  
    //Display processes along with all details 
    printf("\n*********\nRR Quantum = %d\n", quantum);
}

//Function to calculate average time 
void findavgTimePriority( ProcessType plist[], int n) 
{ 
    // sort by priority, then it's just FCFS on the sorted list
    qsort(plist, n, sizeof(ProcessType), my_comparer);

    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
  
    //Display processes along with all details 
    printf("\n*********\nPriority\n");
}

void printMetrics(ProcessType plist[], int n)
{
    int total_wt = 0, total_tat = 0; 
    float awt, att;
    
    printf("\tProcesses\tBurst time\tWaiting time\tTurn around time\n"); 
  
    // Calculate total waiting time and total turn  
    // around time 
    for (int  i=0; i<n; i++) 
    { 
        total_wt = total_wt + plist[i].wt; 
        total_tat = total_tat + plist[i].tat; 
        printf("\t%d\t\t%d\t\t%d\t\t%d\n", plist[i].pid, plist[i].bt, plist[i].wt, plist[i].tat); 
    } 
  
    awt = ((float)total_wt / (float)n);
    att = ((float)total_tat / (float)n);
    
    printf("\nAverage waiting time = %.2f", awt); 
    printf("\nAverage turn around time = %.2f\n", att); 
} 

ProcessType * initProc(char *filename, int *n) 
{
  	FILE *input_file = fopen(filename, "r");
	  if (!input_file) {
		    fprintf(stderr, "Error: Invalid filepath\n");
		    fflush(stdout);
		    exit(0);
	  }

    ProcessType *plist = parse_file(input_file, n);
  
    fclose(input_file);
  
    return plist;
}
  
// Driver code 
int main(int argc, char *argv[]) 
{ 
    int n; 
    int quantum = 2;

    ProcessType *proc_list;
  
    if (argc < 2) {
		   fprintf(stderr, "Usage: ./schedsim <input-file-path>\n");
		   fflush(stdout);
		   return 1;
	   }
    
  // FCFS
    n = 0;
    proc_list = initProc(argv[1], &n);
  
    findavgTimeFCFS(proc_list, n);
    
    printMetrics(proc_list, n);
  
  // SJF
    n = 0;
    proc_list = initProc(argv[1], &n);
   
    findavgTimeSJF(proc_list, n); 
   
    printMetrics(proc_list, n);
  
  // Priority
    n = 0; 
    proc_list = initProc(argv[1], &n);
    
    findavgTimePriority(proc_list, n); 
    
    printMetrics(proc_list, n);
    
  // RR
    n = 0;
    proc_list = initProc(argv[1], &n);
    
    findavgTimeRR(proc_list, n, quantum); 
    
    printMetrics(proc_list, n);
    
    return 0; 
}