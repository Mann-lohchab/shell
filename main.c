#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define GREEN   "\x1b[32m"
#define RESET   "\x1b[0m"

char *history[MAX_HISTORY];
int history_count = 0;
char prompt[64] = "mysh";

// Signal handler
void handle_sigint(int sig) {
    write(STDOUT_FILENO, "\n", 1);
    printf(GREEN "%s> " RESET, prompt);
    fflush(stdout);
}

// Prompt printer
void print_prompt() {
    printf(GREEN "%s> " RESET, prompt);
    fflush(stdout);
}

// Add command to history
void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY)
        history[history_count++] = strdup(cmd);
}

// Show history
void show_history() {
    for (int i = 0; i < history_count; i++)
        printf("%d: %s\n", i + 1, history[i]);
}

// Show help
void show_help() {
    printf("MySH Help:\n");
    printf("  cd <dir>         Change directory\n");
    printf("  exit             Exit the shell\n");
    printf("  history          Show command history\n");
    printf("  !n               Execute nth command from history\n");
    printf("  setprompt <name> Set custom prompt\n");
    printf("  cmd1 | cmd2      Pipe output of cmd1 to input of cmd2\n");
    printf("  cmd > file       Redirect output to file\n");
    printf("  cmd < file       Redirect input from file\n");
    printf("  &                Run command in background\n");
}

// Parse input into args
void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1)
        args[++i] = strtok(NULL, " ");
    args[i] = NULL;
}

// Handle input/output redirection
int handle_redirection(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], ">") == 0 && args[i + 1]) {
            int fd = open(args[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        }
        if (strcmp(args[i], "<") == 0 && args[i + 1]) {
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
    return 0;
}

// Check for background execution
int is_background(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "&") == 0) {
            args[i] = NULL;
            return 1;
        }
    }
    return 0;
}

// Run command from history (!n)
int run_history_command(char *input, char **args) {
    if (input[0] == '!' && strlen(input) > 1) {
        int cmd_num = atoi(&input[1]);
        if (cmd_num > 0 && cmd_num <= history_count) {
            strcpy(input, history[cmd_num - 1]);
            printf("Executing: %s\n", input);
            parse_input(input, args);
            return 1;
        } else {
            printf("No such command in history.\n");
            return -1;
        }
    }
    return 0;
}

// Handle piping (cmd1 | cmd2)
int execute_piped_commands(char **args) {
    int pipe_pos = -1;
    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }
    if (pipe_pos == -1) return 0;

    args[pipe_pos] = NULL;
    char **cmd1 = args;
    char **cmd2 = &args[pipe_pos + 1];

    int pipefd[2];
    pipe(pipefd);
    pid_t p1 = fork();
    if (p1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        execvp(cmd1[0], cmd1);
        perror("execvp"); exit(1);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); close(pipefd[0]);
        execvp(cmd2[0], cmd2);
        perror("execvp"); exit(1);
    }

    close(pipefd[0]); close(pipefd[1]);
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    return 1;
}

// Handle built-in commands
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
    if (strcmp(args[0], "help") == 0) {
        show_help();
        return 1;
    }
    if (strcmp(args[0], "setprompt") == 0) {
        if (args[1]) {
            strncpy(prompt, args[1], sizeof(prompt) - 1);
            prompt[sizeof(prompt) - 1] = '\0';
        } else {
            printf("Usage: setprompt <new_prompt>\n");
        }
        return 1;
    }
    return -1; // Not a built-in
}

// Execute external commands
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

// Save history to ~/.mysh_history
void save_history_to_file() {
    char path[512];
    snprintf(path, sizeof(path), "%s/.mysh_history", getenv("HOME") ? getenv("HOME") : ".");
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < history_count; i++)
        fprintf(f, "%s\n", history[i]);
    fclose(f);
}

// Load history from ~/.mysh_history
void load_history_from_file() {
    char path[512];
    snprintf(path, sizeof(path), "%s/.mysh_history", getenv("HOME") ? getenv("HOME") : ".");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_INPUT];
    while (fgets(line, sizeof(line), f) && history_count < MAX_HISTORY) {
        line[strcspn(line, "\n")] = 0;
        history[history_count++] = strdup(line);
    }
    fclose(f);
}

// Shell loop
void shell_loop() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    signal(SIGINT, handle_sigint);
    load_history_from_file();
    printf("Welcome to MySH! Type 'help' or 'exit'.\n");

    while (1) {
        print_prompt();
        if (!fgets(input, MAX_INPUT, stdin)) {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;

        // Handle !n history command
        int hist_exec = run_history_command(input, args);
        if (hist_exec == -1) continue;
        if (hist_exec == 1) {
            int bg = is_background(args);
            int built = handle_builtin(args);
            if (built == 0) break;
            if (built == 1) continue;
            if (execute_piped_commands(args)) continue;
            execute_command(args, bg);
            continue;
        }

        add_to_history(input);
        parse_input(input, args);

        int bg = is_background(args);
        int built = handle_builtin(args);
        if (built == 0) break;
        if (built == 1) continue;
        if (execute_piped_commands(args)) continue;

        execute_command(args, bg);
    }

    save_history_to_file();
    printf("Goodbye!\n");
}

int main() {
    shell_loop();
    return 0;
}
