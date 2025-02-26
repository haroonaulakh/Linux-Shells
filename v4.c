#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
// livraries to provide command line history and editing features
#include <readline/readline.h>   
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "NewShell@/home/new/:- "
#define HISTORY_SIZE 10

int execute(char* arglist[], int background);
char** tokenize(char* cmdline);
char* read_cmd(char* prompt);
void handle_redirection(char **arglist);
int handle_pipes(char *cmdline);
void sigchld_handler(int sig);
char* get_history_command(int index);

int main() {
    char *cmdline;
    char* prompt = PROMPT;

    // Set up the SIGCHLD handler to reap background processes
    signal(SIGCHLD, sigchld_handler);

    // Initialize history file and set maximum history size
    using_history();
    stifle_history(HISTORY_SIZE);  // 10 here

    while ((cmdline = read_cmd(prompt)) != NULL) {
        if (handle_pipes(cmdline) == 1) {
            free(cmdline);
            continue;
        }

        char** arglist = tokenize(cmdline);
        if (arglist != NULL) {
            int background = 0;

            // Check if the last argument is "&" for background execution
            for (int i = 0; arglist[i] != NULL; i++) {
                if (strcmp(arglist[i], "&") == 0 && arglist[i + 1] == NULL) {
                    background = 1;
                    free(arglist[i]);  // Free the "&" token
                    arglist[i] = NULL; // Set last argument to NULL
                    break;
                }
            }

            handle_redirection(arglist);
            execute(arglist, background);

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
            if (!background) {
                // Wait for the process if it's not running in the background
                waitpid(cpid, &status, 0);
            } else {
                printf("[1] %d\n", cpid);  // Print background process ID
            }
            return 0;
    }
}

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

char* read_cmd(char* prompt) {
    char* cmdline = readline(prompt);
    if (cmdline == NULL) {
        return NULL;
    }

    // Handle history commands (!number)
    // he shell supports commands like !number to
    // recall specific commands from history (!2)
    if (cmdline[0] == '!') {
        int index;
        if (cmdline[1] == '-') {
            index = history_length - atoi(&cmdline[2]);
        } else {
            index = atoi(&cmdline[1]) - 1;
        }

        free(cmdline);
        // If a history command is detected, 
        // the shell retrieves the command from history using
        cmdline = get_history_command(index);
    }

    if (cmdline != NULL && *cmdline) {
        add_history(cmdline); // add each new commmand to the hostory list after it's executed
    }

    return cmdline;
}

char* get_history_command(int index) {
    HIST_ENTRY* entry = history_get(index + 1);
    if (entry != NULL) {
        printf("Repeating command: %s\n", entry->line);
        return strdup(entry->line);
    } else {
        printf("No such command in history.\n");
        return NULL;
    }
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
