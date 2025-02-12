#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024

void shell_loop() {
    char input[MAX_INPUT];

    while (1) {
        printf("\033[1;36mmysh>\033[0m ");  // Shell prompt \033[1;36m
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            break;  // Handle Ctrl+D
        }

        input[strcspn(input, "\n")] = 0;  // Remove newline

        if (strcmp(input, "exit") == 0) {
            break;  // Exit command
        }

        pid_t pid = fork();
        if (pid == 0) { // Child process
            char *args[] = {input, NULL};
            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        } else if (pid > 0) { // Parent process
            wait(NULL);
        } else {
            perror("fork failed");
        }
    }
}

int main() {
    shell_loop(1);
    return 0;
}
