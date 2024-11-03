#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_BACKGROUND_PROCESSES 100

typedef struct {
    pid_t pid;
    char command[MAX_COMMAND_LENGTH];
} BackgroundProcess;

BackgroundProcess background_processes[MAX_BACKGROUND_PROCESSES];
int background_count = 0;

// Function to list all running background jobs
void list_jobs() {
    printf("Background jobs:\n");
    for (int i = 0; i < background_count; i++) {
        printf("[%d] %d %s\n", i+1, background_processes[i].pid, background_processes[i].command);
    }
}

// Function to kill a background process
void kill_job(int job_number) {
    if (job_number > 0 && job_number <= background_count) {
        pid_t pid = background_processes[job_number - 1].pid;
        if (kill(pid, SIGKILL) == 0) {
            printf("Process %d terminated.\n", pid);
            for (int i = job_number - 1; i < background_count - 1; i++) {
                background_processes[i] = background_processes[i + 1];
            }
            background_count--;
        } else {
            perror("Failed to kill process");
        }
    } else {
        printf("Invalid job number.\n");
    }
}

// Function to display available built-in commands
void display_help() {
    printf("Available built-in commands:\n");
    printf("cd <directory>: Change the working directory.\n");
    printf("exit: Terminate the shell.\n");
    printf("jobs: List currently running background processes.\n");
    printf("kill <job_number>: Terminate a background process.\n");
    printf("help: Display this help message.\n");
}

// Function to change the directory
void change_directory(char *path) {
    if (chdir(path) != 0) {
        perror("chdir failed");
    }
}

// Check if a command is built-in and execute it
int execute_builtin_command(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        change_directory(args[1]);
        return 1;
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(args[0], "jobs") == 0) {
        list_jobs();
        return 1;
    } else if (strcmp(args[0], "kill") == 0) {
        if (args[1]) {
            kill_job(atoi(args[1]));
        } else {
            printf("kill: usage: kill <job_number>\n");
        }
        return 1;
    } else if (strcmp(args[0], "help") == 0) {
        display_help();
        return 1;
    }
    return 0;
}

void execute_command(char **args) {
    if (execute_builtin_command(args)) {
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
    } else if (pid == 0) {
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }
}

// Shell loop to handle commands
void shell_loop() {
    char line[MAX_COMMAND_LENGTH];
    char *args[MAX_COMMAND_LENGTH / 2 + 1];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        line[strcspn(line, "\n")] = '\0';

        int i = 0;
        args[i] = strtok(line, " ");
        while (args[i] != NULL) {
            args[++i] = strtok(NULL, " ");
        }

        if (args[0] != NULL) {
            execute_command(args);
        }
    }
}

int main() {
    shell_loop();
    return 0;
}
