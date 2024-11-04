#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h> // provides functionality to handle signals (manage SIGCHLD, which is sent when a child terminates)

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "NewShell@/home/new/:- "

int execute(char* arglist[], int background);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
void handle_redirection(char **arglist);
int handle_pipes(char *cmdline);
void sigchld_handler(int sig);

int main() {
    char *cmdline;
    char* prompt = PROMPT;

    // Set up the SIGCHLD handler to reap background processes (ecnusres no zombie processes)
    signal(SIGCHLD, sigchld_handler);

    while ((cmdline = read_cmd(prompt, stdin)) != NULL) {
        if (handle_pipes(cmdline) == 1) {
            free(cmdline);
            continue;
        }

        char** arglist = tokenize(cmdline);
        if (arglist != NULL) {
            int background = 0;  // check if a command should run in the background.

            // Check if the last argument is "&" for background execution, set backforund to 1
            for (int i = 0; arglist[i] != NULL; i++) {
                if (strcmp(arglist[i], "&") == 0 && arglist[i + 1] == NULL) {
                    background = 1;
                    free(arglist[i]);  // Free the "&" token
                    arglist[i] = NULL; // Set last argument to NULL
                    break;
                }
            }

            handle_redirection(arglist);
            execute(arglist, background); // called with nackground flag to handle background execution

            // Free allocated memory
            for (int j = 0; j < MAXARGS + 1; j++)
                free(arglist[j]);
            free(arglist);
            free(cmdline);
        }
    }
    printf("\n");
    return 0;
}

// This function creates a child process using fork() 
// and executes the command in arglist.
int execute(char* arglist[], int background) {
    int status;
    int cpid = fork();
    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            execvp(arglist[0], arglist);
            perror("Command not found...");
            exit(1);
        default:
            if (!background) {  // if background is 0
                // Wait for the process if it's not running in the background
                // the parent process waits for the child process to complete before continuing.
                waitpid(cpid, &status, 0);
            } else {  // background is 1
                // the child process runs in the background.
                //  parent process does not wait for it to complete
                // 
                printf("[1] %d\n", cpid);  // Print background child process ID
            }
            return 0;
    }
}

// tokenizing 
char** tokenize(char* cmdline) {
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;
    char* token = strtok(cmdline, " \t\n");

    while (token != NULL && argnum < MAXARGS) {
        arglist[argnum] = strdup(token);
        argnum++;
        token = strtok(NULL, " \t\n");
    }
    arglist[argnum] = NULL;
    return arglist;
}

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    int c;
    int pos = 0;
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);

    while ((c = getc(fp)) != EOF) {
        if (c == '\n')
            break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0)
        return NULL;
    cmdline[pos] = '\0';
    return cmdline;
}

void handle_redirection(char **arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "<") == 0) {
            int fd = open(arglist[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("Failed to open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            arglist[i] = NULL;
        } else if (strcmp(arglist[i], ">") == 0) {
            int fd = open(arglist[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("Failed to open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            arglist[i] = NULL;
        }
    }
}

int handle_pipes(char *cmdline) {
    char* commands[MAXARGS];
    char* token = strtok(cmdline, "|");
    int num_cmds = 0;

    while (token != NULL && num_cmds < MAXARGS) {
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }
    if (num_cmds < 2)
        return 0;

    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("Pipe failed");
            exit(1);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        char** arglist = tokenize(commands[i]);
        if (arglist == NULL || arglist[0] == NULL) {
            fprintf(stderr, "Invalid command segment\n");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            if (i > 0) {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < num_cmds - 1) {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }
            execvp(arglist[0], arglist);
            perror("Command execution failed");
            exit(1);
        } else {
            for (int j = 0; arglist[j] != NULL; j++) {
                free(arglist[j]);
            }
            free(arglist);
        }
    }

    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
    return 1;
}

void sigchld_handler(int sig) {
    // Reap all terminated child processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
