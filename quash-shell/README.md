Quash: a small command shell in C

Quash is a simple shell, kind of like a tiny version of bash. It prints a prompt, reads what you type, and either runs one of its own built-in commands or starts a program for you. The whole thing is in one file, shell.c.

To build and run it:

gcc -Wall -Wextra -o shell shell.c
./shell

It compiles with no warnings on my end. Here is everything it can do:

Prompt that shows the current directory, like /home/codio>
Built-ins: cd, pwd, echo, env, setenv, exit
$VAR expansion, so echo $HOME works, and quotes like grep "this"
Running any program on the system (ls, cp, a.out, and so on)
Background jobs with &
Ctrl-C stops the running program but not the shell
Foreground programs get killed if they run longer than 10 seconds
Redirection with < and >, and pipes with | (all three, so I'm going for the extra credit)
How I built it

I did the tasks in order, and each one is just added onto the one before it. Here is how each part works and why I did it that way.

Prompt and tokenizing

The prompt uses getcwd() and then prints >  after it. The tokenizer was more work than I expected. At first I used strtok, but then the assignment's example cat testData | grep "this" has quotes in it, and strtok would pass the quote characters straight to grep, which would then search for "this" with quotes and find nothing. So I wrote my own tokenizer that walks through the line one character at a time. It splits on whitespace, but when it hits a quote it keeps everything inside as one token and drops the quote characters. It edits the line in place, which is fine because I give it a copy of what the user typed (cmd_bak), so the original line stays untouched.

Variables are handled right in the tokenizer too. If a token starts with $, I swap it for the value from getenv() (or an empty string if the variable isn't set). Doing it there means $VAR works for every command, including cd and setenv, without any extra code.

Built-in commands

Each built-in is its own small function. A few details:

cd with no argument goes to $HOME.
env by itself loops through environ and prints everything. env NAME only prints that one value, which is what the assignment example shows.
setenv takes both setenv name=value (the form in the example) and setenv name value (the form in the hints), since the assignment uses both.
exit is handled in main so it can break out of the loop and end the shell.
Running programs (fork, execvp, wait)

For anything that isn't a built-in, the shell calls fork() to make a copy of itself. The copy (the child) calls execvp() to turn into the program the user asked for, and the original (the parent) waits with waitpid() until the child is done. If execvp() fails, it only returns if there was a problem, so right after it I print the error with perror and exit the child. That gives the "execvp() failed: No such file or directory" message from the assignment.

One thing I ran into is that I call fflush(stdout) before every fork(). If there is text sitting in the output buffer, the child gets its own copy of that buffer and prints it again, and I was seeing doubled output when I piped the shell's output somewhere.

Background processes

Before tokenizing, I check whether the line ends with & and cut it off if it does. I do this on the raw string and not on the tokens so the & never ends up in the arguments for execvp. It also works with or without a space (a.out & or a.out&). For a background job the parent just doesn't wait.

That created a different problem though. A child that finishes but never gets waited on turns into a zombie process and sticks around. To fix it, I call waitpid(-1, NULL, WNOHANG) in a loop at the start of every prompt. The WNOHANG part means it doesn't block if nothing has finished, it just collects whatever is done.

Signal handling (Ctrl-C)

The shell sets its own SIGINT handler that only prints a newline, so Ctrl-C doesn't kill it. The child processes have to do the opposite: right after fork() they put SIGINT back to the default (SIG_DFL), so Ctrl-C does kill the program that's running. Background jobs also get moved into their own process group with setpgid(0, 0), because the terminal sends Ctrl-C to the whole foreground group and I didn't want it hitting background jobs.

I used sigaction instead of signal(). On Linux, signal() automatically restarts fgets after a signal, so if you pressed Ctrl-C at an empty prompt, you wouldn't get a new prompt. With sigaction and no SA_RESTART, fgets returns early with the error EINTR. The starter code treats any fgets error as fatal and exits, so I added a check: if the error is EINTR, I clear it and just show the prompt again. I also made waitpid retry if it gets interrupted, so the shell doesn't give up on a foreground program early.

The 10 second timer

When a foreground job starts, the shell calls alarm(10). If the job ends first, I call alarm(0) to cancel it. If the alarm goes off first, the SIGALRM handler calls kill(pid, SIGKILL) on the job. I picked SIGKILL over SIGTERM because a program can ignore SIGTERM, but it can't ignore SIGKILL. The handler needs to know who to kill, so the shell keeps the foreground PIDs in a global array. Background jobs never go in that array, so the timer can't touch them. The limit is a FG_TIMEOUT_SECS constant at the top, so it's easy to change. The shell kills the job silently, since the assignment doesn't say to print anything.

Redirection and pipes

For this part I made a Command struct that holds one command's arguments plus an optional input file and output file. parse_pipeline walks through the tokens and splits them at every |, and it pulls < file and > file out into those fields. It also catches mistakes like ls | or cat > and prints a syntax error instead of crashing.

Then run_pipeline forks one child per command. If there are pipes, it makes them first with pipe(), and each child uses dup2() to connect its stdin and stdout to the neighbors. Files work the same way: open() the file, then dup2() it over stdin or stdout. A file redirect is applied after the pipe connections, so it wins if both are there, like in bash. Every child has to close all the pipe ends it isn't using, and so does the parent. If one is left open somewhere, the reader never sees the end of the input and just hangs. The timer works for pipelines too, since it kills every PID in the foreground array.

Built-ins inside a pipeline (like env | grep PATH) run in the child process. A lone built-in with a redirect, like echo hi > file, runs in the shell itself, since cd and setenv need to change the shell's own state. For those I save stdout with dup(), point it at the file, run the built-in, then put it back.

Problems I ran into
Tokenizer bug. After I added quote support, every command only saw its first word. The cause was that I wrote the '\0' that ends a token, and then checked whether I was at the end of the line. But the '\0' overwrote the space, so it always looked like the end. I fixed it by checking first and writing second. It was confusing to debug because a bare cat started reading the rest of my test script from stdin.
Ctrl-C at the prompt made the shell print "fgets error" and quit, which is the EINTR problem from above.
Hanging pipes when I forgot to close a pipe end in one of the processes.
Doubled output from unflushed buffers before fork().
Testing

I ran the examples from the assignment by hand, and I also wrote some test scripts. For Ctrl-C, I used a small Python script that starts the shell, runs sleep 100, then sends SIGINT to the whole process group (like a terminal does) and checks that the sleep dies and the shell lives. For the timer, I ran sleep 100 and timed it: it was killed at 10.0 seconds. I also ran two 6 second jobs back to back (12 seconds total) to make sure the first job's timer didn't leak over and kill the second one. For redirection I tried cat shell.c > outputfile, wc -l < testData, sort < testData > sorted, cat testData | grep "this", a three-part pipe, built-ins in pipes, and some bad input like cat < nonexistent.

Limits
The |, < and > symbols need spaces around them. a|b won't split.
A quoted "|" still counts as a pipe.
No >> (append), since the assignment didn't ask for it.
Fixed sizes: 1024 characters per line, 128 arguments, 16 commands in a pipeline.
No jobs or fg/bg, so there's no way to bring a background job back.

