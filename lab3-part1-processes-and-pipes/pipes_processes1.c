// 2-way pipe: P1 sends a string to P2, P2 appends "howard.edu", prints it,
// prompts for a second string, appends it, and sends the result back to P1,
// which appends "gobison.org" and prints the final string.
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>

#define INPUT_SIZE  100   // max length of one user input
#define BUF_SIZE    300   // large enough for two inputs + both fixed strings

// Read a line from stdin (spaces allowed), strip the newline,
// and discard anything beyond the buffer so it can't overflow.
void read_line(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL)
    {
        buf[0] = '\0';
        return;
    }
    char *nl = strchr(buf, '\n');
    if (nl)
        *nl = '\0';
    else
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

// Append src to dst without exceeding dst_size.
void append_str(char *dst, const char *src, int dst_size)
{
    int k = strlen(dst);
    int i;
    for (i = 0; src[i] != '\0' && k < dst_size - 1; i++)
        dst[k++] = src[i];
    dst[k] = '\0';
}

int main()
{
    int fd1[2];  // P1 -> P2
    int fd2[2];  // P2 -> P1

    char fixed_str1[] = "howard.edu";
    char fixed_str2[] = "gobison.org";
    char input_str[INPUT_SIZE];
    pid_t p;

    if (pipe(fd1) == -1)
    {
        perror("Pipe Failed");
        return 1;
    }
    if (pipe(fd2) == -1)
    {
        perror("Pipe Failed");
        return 1;
    }

    printf("Enter a string to concatenate:");
    fflush(stdout);
    read_line(input_str, INPUT_SIZE);

    p = fork();

    if (p < 0)
    {
        perror("fork Failed");
        return 1;
    }

    // Parent process (P1)
    else if (p > 0)
    {
        char final_str[BUF_SIZE];

        close(fd1[0]);
        close(fd2[1]);

        if (write(fd1[1], input_str, strlen(input_str) + 1) == -1)
        {
            perror("write to P2 failed");
            return 1;
        }
        close(fd1[1]);

        wait(NULL);

        if (read(fd2[0], final_str, BUF_SIZE) <= 0)
        {
            perror("read from P2 failed");
            return 1;
        }
        close(fd2[0]);

        append_str(final_str, fixed_str2, BUF_SIZE);
        printf("Concatenated string %s\n", final_str);
    }

    // Child process (P2)
    else
    {
        char concat_str[BUF_SIZE];
        char second_str[INPUT_SIZE];

        close(fd1[1]);
        close(fd2[0]);

        if (read(fd1[0], concat_str, BUF_SIZE) <= 0)
        {
            perror("read from P1 failed");
            exit(1);
        }
        close(fd1[0]);

        append_str(concat_str, fixed_str1, BUF_SIZE);
        printf("Concatenated string %s\n", concat_str);

        printf("Enter another string to concatenate:");
        fflush(stdout);
        read_line(second_str, INPUT_SIZE);

        append_str(concat_str, second_str, BUF_SIZE);

        if (write(fd2[1], concat_str, strlen(concat_str) + 1) == -1)
        {
            perror("write to P1 failed");
            exit(1);
        }
        close(fd2[1]);

        exit(0);
    }

    return 0;
}