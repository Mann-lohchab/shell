#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100

char *history[MAX_HISTORY];
int history_count = 0;

void handle_sigint(int sig) {
    write(STDOUT_FILENO, "\n~> ", 7);
}

void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY)
        history[history_count++] = strdup(cmd);
}

void show_history() {
    for (int i = 0; i < history_count; i++)
        printf("%d: %s\n", i + 1, history[i]);
}

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}

int handle_redirection(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], ">") == 0 && args[i+1]) {
            int fd = open(args[i+1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        }
        if (strcmp(args[i], "<") == 0 && args[i+1]) {
            int fd = open(args[i+1], O_RDONLY);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
    return 0;
}

int is_background(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "&") == 0) {
            args[i] = NULL;
            return 1;
        }
    }
    return 0;
}

int handle_builtin(char **args) {
    if (!args[0]) return 1;
    if (strcmp(args[0], "exit") == 0) return 0;
    if (strcmp(args[0], "cd") == 0) {
        chdir(args[1] ? args[1] : getenv("HOME"));
        return 1;
    }
    if (strcmp(args[0], "history") == 0) {
        show_history();
        return 1;
    }
    return -1;
}

void execute_command(char **args, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        handle_redirection(args);
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (!background)
            waitpid(pid, NULL, 0);
        else
            printf("[Background pid %d]\n", pid);
    } else {
        perror("fork");
    }
}

void shell_loop() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    signal(SIGINT, handle_sigint);
    printf("Welcome to MySH! Type 'exit' to quit.\n");

    while (1) {
        printf("mysh> ");
        if (!fgets(input, MAX_INPUT, stdin)) {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;

        add_to_history(input);
        parse_input(input, args);

        int bg = is_background(args);
        int built = handle_builtin(args);
        if (built == 0) break;
        if (built == 1) continue;

        execute_command(args, bg);
    }

    printf("Goodbye!\n");
}

int main() {
    shell_loop();
    return 0;
}
