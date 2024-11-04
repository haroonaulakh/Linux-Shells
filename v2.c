#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LEN 512  // max length of input command
#define MAXARGS 10   // max number of arguments in command
#define ARGLEN 30    // max length of each argument
#define PROMPT "Newshell@/home/new/:- "

int execute(char* arglist[]);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
void handle_redirection(char **arglist);
int handle_pipes(char *cmdline);

// main loop
// reads a command
// processes pipes (if any)
// tokenize the command
// executes it
int main() {   
    char *cmdline;
    char* prompt = PROMPT;   
    
    while((cmdline = read_cmd(prompt, stdin)) != NULL) {
        if (handle_pipes(cmdline) == 1) {
            free(cmdline);
            continue;
        }
        
        char** arglist = tokenize(cmdline);
        if (arglist != NULL) {
            handle_redirection(arglist);  // handling any redirection
            execute(arglist);

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

// forks a new process to execute a command specified by arglist
// child process calls execvp to execute the command
// while parent waits for child to finish
int execute(char* arglist[]) {
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
            waitpid(cpid, &status, 0);
            return 0;
    }
}

// tokenization
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


// reading command
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


// handling redirection
// looks redirection operators (<,>) in command arguments
void handle_redirection(char **arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        // if < is found, opens specified file for reading,
        // redirects STDIN to it
        if (strcmp(arglist[i], "<") == 0) {
            int fd = open(arglist[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("Failed to open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            arglist[i] = NULL;
        // if < is found, opens file for writing (create if it doesn't exist),
        // redirects STDOUT to it
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



// handle command with pipes (|)
// this functoin splits the command by | to separate
// the commands for each stage of pipeline
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
    
    // creates pipes for each pair of commands, 
    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("Pipe failed");
            exit(1);
        }
    }

    
    // sets up redirection ad needed b/w process
    // uses execvp to execute each command segment
     // parent waits for each child process to complete before returning
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
