#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_INPUT 255
#define MAX_ARGS 64
#define MAX_HISTORY 100

// Colors
#define GREEN  "\x1b[32m"
#define BLUE   "\x1b[34m"
#define WHITE  "\x1b[37m"
#define RESET  "\x1b[0m"

char *history[MAX_HISTORY];
int history_count = 0;
char prompt[64] = "mysh";

// Print prompt
void print_prompt() {
    printf(GREEN "%s> " RESET, prompt);
    fflush(stdout);
}

// Handle Ctrl+C
void handle_sigint(int sig) {
    write(STDOUT_FILENO, "\n", 1);
    print_prompt();
}

// Add to history
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
    puts("MySH Help:");
    puts("  cd <dir>         Change directory");
    puts("  exit             Exit the shell");
    puts("  history          Show command history");
    puts("  !n               Execute nth command from history");
    puts("  setprompt <name> Set custom prompt");
    puts("  ls               List files (colored)");
    puts("  cmd1 | cmd2      Pipe commands");
    puts("  cmd > file       Redirect output");
    puts("  cmd < file       Redirect input");
    puts("  cmd &            Run in background");
}

// Custom ls with color
void custom_ls() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "./%s", entry->d_name);
        if (stat(path, &st) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode))
            printf(BLUE "%s  " RESET, entry->d_name);
        else if (st.st_mode & S_IXUSR)
            printf(GREEN "%s  " RESET, entry->d_name);
        else
            printf(WHITE "%s  " RESET, entry->d_name);
    }

    printf("\n");
    closedir(dir);
}

// Parse input
void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1)
        args[++i] = strtok(NULL, " ");
    args[i] = NULL;
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

// Handle redirection
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

// Execute piped commands
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
        perror("execvp");
        exit(1);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); close(pipefd[0]);
        execvp(cmd2[0], cmd2);
        perror("execvp");
        exit(1);
    }

    close(pipefd[0]); close(pipefd[1]);
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    return 1;
}

// Run !n history command
int run_history_command(char *input, char **args) {
    if (input[0] == '!' && strlen(input) > 1) {
        int n = atoi(&input[1]);
        if (n > 0 && n <= history_count) {
            strcpy(input, history[n - 1]);
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
            printf("Usage: setprompt <new_name>\n");
        }
        return 1;
    }

    if (strcmp(args[0], "ls") == 0) {
        custom_ls();
        return 1;
    }

    return -1;
}

// Execute external command
void execute_command(char **args, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        handle_redirection(args);
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (!background) waitpid(pid, NULL, 0);
        else printf("[Background pid %d]\n", pid);
    } else {
        perror("fork");
    }
}

// Shell loop
void shell_loop() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    signal(SIGINT, handle_sigint);
    printf("Welcome to MySH! Type 'help' or 'exit'.\n");

    while (1) {
        print_prompt();
        if (!fgets(input, MAX_INPUT, stdin)) {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;

        int hist_exec = run_history_command(input, args);
        if (hist_exec == -1) continue;
        if (hist_exec == 1) {
            int bg = is_background(args);
            int b = handle_builtin(args);
            if (b == 0) break;
            if (b == 1) continue;
            if (execute_piped_commands(args)) continue;
            execute_command(args, bg);
            continue;
        }

        add_to_history(input);
        parse_input(input, args);

        int bg = is_background(args);
        int b = handle_builtin(args);
        if (b == 0) break;
        if (b == 1) continue;
        if (execute_piped_commands(args)) continue;

        execute_command(args, bg);
    }

    printf("Goodbye!\n");
}

// Entry point
int main() {
    shell_loop();
    return 0;
}
