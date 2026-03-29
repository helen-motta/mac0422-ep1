#define _POSIX_C_SOURCE 200112L
#include "imesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

char* get_user_name() {
    uid_t id = getuid();
    struct passwd *pwd = getpwuid(id);

    if (pwd == NULL) {
        perror("imesh: erro ao obter usuario");
        return "desconhecido";
    }
    return pwd->pw_name;
}

int get_host_name(char* buffer, size_t size) {
    return gethostname(buffer, size);
}

char* get_current_dir(char* buffer, size_t size) {
    char* cdir = getcwd(buffer, size);
    if (cdir == NULL) {
        perror("imesh: erro ao obter diretorio atual");
        return "desconhecido";
    }
    return cdir;
}

int print_prompt_line() {
    char hostname[128];
    char cwd[1024];

    get_host_name(hostname, sizeof(hostname));
    get_current_dir(cwd, sizeof(cwd));

    printf("[%s@%s:%s]$ ", get_user_name(), hostname, cwd);
    fflush(stdout);
    
    return 0;
}

char* handle_pwd_command() {
    static char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) return cwd;
    return "erro ao obter diretorio";
}

char* handle_date_command() {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)time(NULL));
    return buffer;
}

void handle_kill_command(char* cmd) {
    int pid;
    int sinal = 9; 
    char* args[10]; 
    int i = 0;

    char* token = strtok(cmd, " ");
    while (token != NULL && i < 9) {
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[1] == NULL) return; 

    if (args[2] != NULL) {
        sinal = atoi(args[1]);
        pid = atoi(args[2]);
        
        if (sinal < 0) {
            sinal *= -1; 
        }
    } else {
        pid = atoi(args[1]);
    }

    if (kill(pid, sinal) == -1) {
        if (errno == ESRCH) {
            printf("imesh: kill: (%d) - No such process\n", pid);
        } 
        else if (errno == EPERM) {
            printf("imesh: kill: (%d) - Operation not permitted\n", pid);
        } 
        else {
            perror("imesh: kill: erro inesperado");
        }
    } 
}

int handle_execute(char *path, char **args) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Erro ao fazer fork");
        return -1;
    }

    if (pid == 0) {
        execv(path, args);
        perror("Erro ao executar execv");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
    return 0;
}

int main() {
    char *cmd = NULL;

    while (1) {
        print_prompt_line();

        // Substitui scanf por readline para capturar entrada do usuário
        cmd = readline("");
        if (cmd == NULL) {
            printf("\n");
            break;
        }

        // Adiciona o comando ao histórico, se não for vazio
        if (strlen(cmd) > 0) {
            add_history(cmd);
        }

        if (strcmp(cmd, "pwd") == 0) {
            printf("%s\n", handle_pwd_command());
        }
        else if (strcmp(cmd, "date +%s") == 0) {
            printf("%s\n", handle_date_command());
        }
        else if (strncmp(cmd, "kill", 4) == 0) {
            handle_kill_command(cmd);
        }
        else if (strcmp(cmd, "/bin/ls -1aF --color=never") == 0) {
            char *args[] = {"/bin/ls", "-1aF", "--color=never", NULL};
            handle_execute("/bin/ls", args);
        } 
        else if (strcmp(cmd, "/bin/top -b -n 1 -p 1") == 0) {
            char *args[] = {"/bin/top", "-b", "-n", "1", "-p", "1", NULL};
            handle_execute("/bin/top", args);
        } 
        else if (strncmp(cmd, "./ep1", 5) == 0) {
            printf("%s", cmd);
            char *args[5]; 
            
            char *token = strtok(cmd, " ");
            int i = 0;
            while (token != NULL && i < 4) {
                args[i] = token;
                token = strtok(NULL, " ");
                i++;
            }
            args[i] = NULL;

            handle_execute("./ep1", args);
        }
        else if (strcmp(cmd, "exit") == 0) {
            free(cmd);
            break;
        }
        else {
            printf("imesh: comando desconhecido: %s\n", cmd);
        }
        free(cmd);
    }
    return 0;
}