#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "string.h"
#include "unistd.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "sys/wait.h"
#include <signal.h>
#include <ctype.h>

// Global variables:
int k = 0; // checks if the exit command has been entered by user

/**************************************************
 *
 * List of background processes
 *
 **************************************************/

int capacity = 100;

struct Background_pids
{
    int size;
    pid_t *array; // list of child processes
};

struct Background_pids background_pids;

int find(pid_t pid)
{
    for (int i = 0; i < background_pids.size; i++)
    {
        if (background_pids.array[i] == pid)
        {
            return i;
        }
    }
    return -1;
}

void add_pid(pid_t pid)
{
    background_pids.array[background_pids.size] = pid;
    background_pids.size++;
    if (background_pids.size > capacity)
    {
        background_pids.array = realloc(background_pids.array, sizeof(pid_t) * capacity * 2);
        capacity *= 2;
    }
}

void delete (pid_t pid)
{
    int index = -1;
    for (int i = 0; i < background_pids.size; i++)
    {
        if (background_pids.array[i] == pid)
        {
            index = i;
            break;
        }
    }
    if (index != -1)
    {
        memmove(&background_pids.array[index + 1], &background_pids.array[index], sizeof(int));
        background_pids.size--;
    }
}

int isEmpty()
{

    for (int i = 0; i < background_pids.size; i++)
    {
        pid_t pid = background_pids.array[i];
        int status = 0;

        // if its finished, remove from list
        waitpid(pid, &status, WNOHANG);

        if (kill(pid, 0) == -1)
        {
            // First of all kill may fail with EPERM if we run as a different user and we have no access, so let's make sure the errno is ESRCH (Process not found!)
            delete (pid);
        }
    }

    if (background_pids.size == 0)
    {
        return 1;
    }

    return 0;
}
// used for getting input from the user
char *readline()
{
    int c; // int not char
    int index = 0;
    int inputsize = 1024;
    char *input = malloc(sizeof(char) * inputsize);

    while (1)
    {
        c = getchar();

        if (c == EOF)
        {
            input[index] = '\0';
            return input;
        }
        if (c == '\n')
        {
            input[index] = '\n';
            index++;
            input[index] = '\0';
            return input;
        }
        else
        {
            input[index] = c;
        }
        index++;

        // increase size of input
        if (index >= inputsize)
        {
            inputsize += 1024;
            input = realloc(input, inputsize * sizeof(char));
            if (!input)
            {
                free(input);
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/**************************************************
 *
 * Functions for handling special characters
 *
 **************************************************/

// ltrim removes spaces at the beggining of the string
char *ltrim(char *str, const char *seps)
{
    size_t totrim;
    if (seps == NULL)
    {
        seps = "\t\n\v\f\r ";
    }
    totrim = strspn(str, seps);
    if (totrim > 0)
    {
        size_t len = strlen(str);
        if (totrim == len)
        {
            str[0] = '\0';
        }
        else
        {
            memmove(str, str + totrim, len + 1 - totrim);
        }
    }
    return str;
}
// rtrim removes the spaces at the end of a string
char *rtrim(char *str, const char *seps)
{
    int i;
    if (seps == NULL)
    {
        seps = "\t\n\v\f\r ";
    }
    i = strlen(str) - 1; // segmentation fault happening here
    while (i >= 0 && strchr(seps, str[i]) != NULL)
    {
        str[i] = '\0';
        i--;
    }
    return str;
}

// combines rtrim and ltrim and trim the spaces from the beggining and from the end
char *trim(char *str, const char *seps)
{
    return ltrim(rtrim(str, seps), seps);
}

// findDelimiter checks for operators that are used in shell
int findDelimiter(char *line)
{
    int length = strlen(line);

    for (int i = 0; i < length; i++)
    {
        if (line[i] == ';')
        {
            return 1;
        }
        if (line[i] == '&' && line[i + 1] == '&')
        {
            return 2;
        }
        if (line[i] == '|' && line[i + 1] == '|')
        {
            return 3;
        }
        if (line[i] == '|' && line[i + 1] != '|')
        {
            return 4;
        }
        if (line[i] == '&' && line[i + 1] != '&')
        {
            return 5;
        }
    }
    return -1; // no delimiters found
}

/**************************************************
 *
 * Parsers
 *
 **************************************************/

void parse(char *line, char **argv)
{
    while (*line != '\0')
    {
        while (*line == ' ' || *line == '\t' || *line == '\n')
        {
            *line++ = '\0';
        }

        *argv++ = trim(line, " \""); // save the argument position

        while (*line != '\0' && *line != ' ' && *line != '\t' && *line != '\n')
        {
            line++; // skip the argument until
        }
    }
    *argv = '\0'; // mark the end of argument list
}

/**************************************************
 *
 * Functions for executing processes
 *
 **************************************************/

int check_for_errors(char **argv)
{
    int idx = 0;
    char *input_file = "";
    char *output_file = "";
    int ins = 0;
    int outs = 0;
    int echo_called = 0;
    while (argv[idx])
    {
        if (strcmp(argv[idx], "echo") == 0)
        {
            echo_called = 1;
        }
        if (*argv[idx] == '>' && argv[idx + 1])
        {
            output_file = argv[idx + 1];
            outs++;
        }
        if (*argv[idx] == '<' && argv[idx + 1])
        {
            input_file = argv[idx + 1];
            ins++;
        }
        idx++;
    }

    if (strcmp(input_file, output_file) == 0 && strcmp(input_file, "") != 0)
    {
        printf("Error: input and output files cannot be equal!\n");
        return 0;
    }

    if (echo_called == 1)
    {
        return 1;
    }

    if ((ins > 1 || outs > 1))
    {
        printf("Error: invalid syntax!\n");
        return 0;
    }

    idx = 0;
    while (argv[idx])
    {
        if (*argv[idx] == '<' && argv[idx + 1] == NULL)
        {
            printf("Error: invalid syntax!\n");
            return 0;
        }
        if (*argv[idx] == '>' && argv[idx + 1] == NULL)
        {
            printf("Error: invalid syntax!\n");
            return 0;
        }
        idx++;
    }

    return 1;
}

int input_output(char **argv, int idx, int fd, int echo_called)
{
    // check for <> symbols
    while (argv[idx])
    {
        // parse args for '<' or '>' and filename
        if (*argv[idx] == '>' && argv[idx + 1])
        {
            if ((fd = open(argv[idx + 1], O_WRONLY | O_CREAT, S_IRWXU)) == -1)
            {
                if (echo_called == 0)
                {
                    close(fd);
                    printf("Error: invalid syntax!\n");
                    return 0;
                }
                else
                {
                    break;
                }
            }
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
            argv[idx] = '\0';
            argv[idx + 1] = '\0';
            idx++;
        }
        else if (*argv[idx] == '<' && argv[idx + 1])
        {
            if ((fd = open(argv[idx + 1], O_RDONLY)) == -1)
            {
                if (echo_called == 0)
                {
                    close(fd);
                    printf("Error: invalid syntax!\n");
                    return 0;
                }
                else
                {
                    break;
                }
            }
            dup2(fd, 0);
            close(fd);
            argv[idx] = '\0';
            argv[idx + 1] = '\0';
            idx++;
        }
        idx++;
    }
    return fd;
}

/**************************************************
 *
 * Signal Handlers
 *
 **************************************************/

void int_handler(int num)
{
    // num = SIGINT = ctrl + C
    if (isEmpty() == 0)
    {
        // background process list is not empty
        printf("Error: there are still background processes running!\n");
    }
    else
    {
        exit(0);
    }
}

void chld_handler(int num, siginfo_t *info, void *ucontext)
{
    // num = SIGCHLD = sent to parent when child terminates
    int status;
    pid_t pid = info->si_pid; // investigate the siginfo_t struct to find the process ID of the terminated child.

    while (waitpid(pid, &status, WNOHANG) > 0)
    { // check that child is actually terminated
        if (find(pid) != -1)
        {
            delete (pid); // update background process list
        }
    }
}

int execute(char **argv, int background)
{
    if (check_for_errors(argv) == 0)
    {
        return 0;
    }
    pid_t pid;
    int status = 0, fd = 0; // idx = 0
    int echo_called = 0;

    if ((pid = fork()) < 0) // fork a child process
    {
        printf("Error: forking child process failed!\n");
        return 1;
    }
    else if (pid == 0)
    {
        if (strcmp(argv[0], "echo") == 0)
        {
            echo_called = 1;
        }
        fd = input_output(argv, 0, fd, echo_called); // check for <>

        if (execvp(*argv, argv) < 0)
        {
            close(fd);
            printf("Error: command not found!\n");
            exit(status - 1);
            return status; // don't terminate shell, keep going with accepting input
        }
    }
    else
    {
        // parent
        if (background == 1)
        {
            add_pid(pid); // add to background list
        }
        else // else dont wait for it to finish
        {
            while (wait(&status) != pid)
                ;
        }
    }
    return status;
}

int execute_cd(char **argv)
{
    if (argv[0] != NULL)
    {
        if (strcmp(argv[0], "cd") == 0)
        {
            if (argv[1] == NULL)
            {
                printf("Error: cd requires folder to navigate to!\n");
                return 0;
            }
            else
            {
                if (chdir(argv[1]) != 0)
                {
                    printf("Error: cd directory not found!\n");
                    return 0;
                }
            }
            return 1;
        }
        return -1;
    }
    return -1; // wouldn't happen
}

// Function where the piped system commands is executed
void execArgsPiped(char **parsed, char **parsedpipe)
{
    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t p1, p2;

    if (parsedpipe[0] == NULL)
    {
        printf("Error: invalid syntax!\n");
        return;
    }
    if (pipe(pipefd) < 0)
    {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0)
    {
        printf("\nCould not fork");
        return;
    }

    if (p1 == 0)
    {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        int echo_called = 0;
        if (strcmp(parsed[0], "echo") == 0)
        {
            echo_called = 1;
        }
        int fd = input_output(parsed, 0, 0, echo_called); // check for <>

        if (execvp(parsed[0], parsed) < 0)
        {
            close(fd);
            printf("\nCould not execute command 1..");
            exit(0);
        }
    }
    else
    {
        // Parent executing
        p2 = fork();

        if (p2 < 0)
        {
            printf("\nCould not fork");
            return;
        }

        // Child 2 executing..
        // It only needs to read at the read end
        if (p2 == 0)
        {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            int echo_called = 0;
            if (strcmp(parsedpipe[0], "echo") == 0)
            {
                echo_called = 1;
            }
            int fd = input_output(parsedpipe, 0, 0, echo_called); // check for <>

            if (execvp(parsedpipe[0], parsedpipe) < 0)
            {
                close(fd);
                printf("\nCould not execute command 2..");
                exit(0);
            }
        }
        else
        {
            // parent executing, waiting for two children
            close(pipefd[0]);     // [STDIN -> terminal_input, STDOUT -> terminal_output, fd[0] -> pipe_input] (of the parent process)
            close(pipefd[1]);     // [STDIN -> terminal_input, STDOUT -> terminal_output]                      (of the parent process)
            waitpid(-1, NULL, 0); // As the parent process - we wait for a process to die (-1) means I don't care which one
            waitpid(-1, NULL, 0);
        }
    }
    return;
}

/**************************************************
 *
 * Functions for determining order of execution
 *
 **************************************************/

void parseSpace(char *str, char **parsed)
{
    int i;

    for (i = 0; i < 100; i++)
    {
        parsed[i] = strsep(&str, " \n");

        if (parsed[i] == NULL)
            break;
        if (strlen(parsed[i]) == 0)
            i--;
    }
}

void processString(char *line, char **argv1, char **argv2)
{

    char *line_split[2];
    int index = 0;
    char *pointer = strsep(&line, "|");

    while (pointer != NULL)
    {
        line_split[index] = pointer;
        pointer = strsep(&line, "|");
        index++;
    }

    if (index == 0)
    {
        // no | symbol, something went wrong, shouldn't happen
    }
    else if (index == 2)
    {
        // regular case
        parse(line_split[0], argv1);
        parse(line_split[1], argv2);
    }
    else
    {
        // more than one | symbol. not sure if this is possible
    }
}

char *multi_tok(char *line, char *delimiter);

void solve(char *line)
{
    char *tok;
    char *argv[64];
    char *argvPiped[100];
    int flag = findDelimiter(line);

    trim(line, " \"\n");

    if (strcmp(line, "exit") == 0)
    {
        if (isEmpty() == 1)
        {
            k = 1;
            return;
        }
        else
        {
            printf("Error: there are still background processes running!\n");
            waitpid(-1, NULL, 0);
            return;
        }
    }
    if (strcmp(line, "jobs") == 0)
    {
        if (isEmpty() == 0)
        {
            if (background_pids.size > 1)
            {
                for (int i = background_pids.size; i > 0; i--)
                {
                    printf("Process running with index %d\n", i);
                    delete (background_pids.array[i - 1]);
                }
            }
            else
            {
                for (int i = background_pids.size; i > 0; i--)
                {
                    printf("Process running with index %d\n", i);
                }
            }
        }
        else
        {
            printf("No background processes!\n");
        }
        return;
    }
    // if it enters multi_tok solve it's called again
    switch (flag)
    {
    case -1: // no delimiters
        parse(line, argv);
        if (argv[0] != NULL)
        {
            int i;
            for (i = 0; i < sizeof(argv); i++)
            {
                if (argv[i] != NULL)
                {
                    trim(argv[i], " \"\n");
                }
                else
                {
                    break;
                }
            }
        }
        if (argv[0] != NULL)
        {
            // check for keywords: cd and kill

            if (strcmp(argv[0], "cd") == 0)
            {
                if (argv[1] == NULL)
                {
                    printf("Error: cd requires folder to navigate to!\n");
                }
                else
                {
                    if (chdir(argv[1]) != 0)
                    {
                        printf("Error: cd directory not found!\n");
                    }
                }

                // kill: kill {idx} {sig=SIGTERM}
            }
            else if (strcmp(argv[0], "kill") == 0)
            {
                if (argv[1] == NULL)
                {
                    printf("Error: command requires an index!\n");
                }
                else
                {
                    if (isdigit(*argv[1]))
                    {
                        int index = (int)strtol(argv[1], NULL, 10);

                        if (index > 0 && index <= background_pids.size)
                        {
                            pid_t pid = background_pids.array[index - 1];

                            // next character should be signal
                            if (argv[2] == NULL)
                            {
                                // default SIGTERM = 9
                                kill(pid, 9);
                                delete (pid);
                            }
                            else
                            {
                                int signal = (int)strtol(argv[2], NULL, 10);
                                if (signal > 0 && signal <= 31)
                                {
                                    kill(-pid, signal);
                                    delete (pid);
                                }
                                else
                                {
                                    printf("Error: invalid signal provided!\n");
                                }
                            }
                        }
                        else
                        {
                            printf("Error: this index is not a background process!\n");
                        }
                    }
                    else
                    {
                        printf("Error: invalid index provided!\n");
                    }
                }
            }
            else
            {
                execute(argv, 0);
            }
        }
        break;
    case 1: //;
        tok = multi_tok(line, ";");
        break;
    case 2: //&&
        tok = multi_tok(line, "&&");
        break;
    case 3: //||
        tok = multi_tok(line, "||");
        break;
    case 4: // |
        processString(line, argv, argvPiped);
        execArgsPiped(argv, argvPiped);
        break;
    case 5: // &
        tok = multi_tok(line, "&");
        break;
    }
}

// used for delimiting strings by multicharacter tokens
char *multi_tok(char *line, char *delimiter)
{
    char *argv[64];
    static char *string;
    if (line != NULL)
    {
        string = line;
    }

    if (string == NULL)
    {
        return string;
    }
    // pointer to start of delimiter
    char *end = strstr(line, delimiter);

    if (end == NULL)
    {
        char *tok = string;
        string = NULL;
        return tok;
    }

    char *tok = string;

    *end = '\0';
    string = end + strlen(delimiter);
    // tok = before delimiter
    // string = after delimiter
    parse(tok, argv);

    if (strcmp(delimiter, ";") == 0)
    {
        if (execute_cd(argv) == 1)
        {
        }
        else
        {
            execute(argv, 0);
        }
        // solve(rest of input)
        solve(string);
    }
    if (strcmp(delimiter, "&&") == 0)
    {
        if (execute_cd(argv) == 1)
        {
            solve(string);
        }
        else if (execute(argv, 0) == 0) // only if it worked
        {
            solve(string); // solve(rest of input)
        }
    }

    if (strcmp(delimiter, "&") == 0)
    {
        execute(argv, 1); // set background to true, hence don't wait
        solve(string);    // continue with rest
    }

    if (strcmp(delimiter, "||") == 0)
    {
        if (execute_cd(argv) == 0)
        {
            solve(string);
        }
        else if (execute(argv, 0) != 0) // only if it didnt work
        {
            solve(string); // solve(rest of input)
        }
        else if (strstr(string, ";") != NULL) // if ; later on then continue
        {
            string = strtok(string, ";");
            string = strtok(NULL, "");
            if (string != NULL)
            {
                trim(string, " \"");
                if (string != NULL)
                    solve(string);
            }
        }
    }

    return tok;
}

/**************************************************
 *
 * Main
 *
 **************************************************/

int main(int argc, const char *argv[], char **envp)
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    background_pids.size = 0;
    background_pids.array = malloc(sizeof(pid_t) * capacity);

    // signal handlers:

    struct sigaction sigint = {0};
    sigint.sa_handler = int_handler;
    sigint.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sigint.sa_mask);
    sigaction(SIGINT, &sigint, NULL); // ctrl + C

    struct sigaction sigchld = {0};
    sigchld.sa_sigaction = chld_handler;
    sigchld.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sigchld.sa_mask);
    sigaction(SIGCHLD, &sigchld, NULL); // send to parent when child terminates

    while (1 && k == 0)
    {
        char *line = readline();

        if (strcmp(line, "exit") == 0 || line[0] == '\0' || k == 1)
        {
            k = 1;
            if (isEmpty() == 1)
            {
                free(background_pids.array);
                free(line);
                return EXIT_SUCCESS;
            }
        }
        // recursive function used for every part of the string delimited by token
        solve(line);

        // free the memory
        free(line);
    }
    free(background_pids.array);
    return EXIT_SUCCESS;
}