#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_COMMANDS 10
#define MAX_ARGS 15
#define MAX_REDIRECTIONS 3   // 0: stdin, 1: stdout, 2: stderr

/* Global variables to store a command's arguments and redirection files */
char *argvv[MAX_ARGS];
char *filev[MAX_REDIRECTIONS]; // filev[0]: input, filev[1]: output, filev[2]: error
int background = 0;

/* Helper: Trim trailing whitespace characters (including CR) in a string */
void trim_token(char *str) {
    if (!str)
        return;
    int len = strlen(str);
    while(len > 0 && (str[len-1] == '\r' || str[len-1] == '\n' ||
                      str[len-1] == ' '  || str[len-1] == '\t')) {
        str[len-1] = '\0';
        len--;
    }
}

/* Tokenize a string using a delimiter and return the number of tokens */
int tokenizar_linea(char *linea, const char *delim, char *tokens[], int max_tokens) {
    int i = 0;
    char *token = strtok(linea, delim);
    while (token != NULL && i < max_tokens - 1) {
        tokens[i++] = token;
        token = strtok(NULL, delim);
    }
    tokens[i] = NULL;
    return i;
}

/* Process redirections in the given args array.
   Sets filev[] for redirections and removes the redirection tokens from args.
   Also trims the file name tokens.
*/
void procesar_redirecciones(char *args[]) {
    filev[0] = filev[1] = filev[2] = NULL;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            filev[0] = args[i+1];
            if (filev[0])
                trim_token(filev[0]);
            args[i] = NULL;
            args[i+1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            filev[1] = args[i+1];
            if (filev[1])
                trim_token(filev[1]);
            args[i] = NULL;
            args[i+1] = NULL;
        } else if (strcmp(args[i], "!>") == 0) {
            filev[2] = args[i+1];
            if (filev[2])
                trim_token(filev[2]);
            args[i] = NULL;
            args[i+1] = NULL;
        }
    }
}

/* Process a command line.
   Splits the line into pipeline segments (commands separated by "|")
   and returns the number of commands.
   The background flag is set if the last command ends with "&".
*/
int procesar_linea(char *linea, char *comandos[]) {
    int num_cmds = tokenizar_linea(linea, "|", comandos, MAX_COMMANDS);
    background = 0;
    char *amp = strchr(comandos[num_cmds - 1], '&');
    if (amp != NULL) {
        background = 1;
        *amp = '\0';  // Remove the '&' character
        trim_token(comandos[num_cmds - 1]);
    }
    return num_cmds;
}

/* Execute a single command (after tokenizing it into argvv and processing redirections) */
int ejecutar_comando(char *cmd) {
    char cmdCopy[MAX_LINE];
    strncpy(cmdCopy, cmd, MAX_LINE);
    cmdCopy[MAX_LINE - 1] = '\0';
    int num_args = tokenizar_linea(cmdCopy, " \t\n", argvv, MAX_ARGS);
    if (num_args == 0) {
        return -1; // no command found
    }
    // Trim all tokens
    for (int i = 0; i < num_args; i++) {
        trim_token(argvv[i]);
    }
    procesar_redirecciones(argvv);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        return -1;
    } else if (pid == 0) {
        if (filev[0] != NULL) {
            int fd_in = open(filev[0], O_RDONLY);
            if (fd_in < 0) { perror("open input file"); exit(-1); }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if (filev[1] != NULL) {
            int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) { perror("open output file"); exit(-1); }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        if (filev[2] != NULL) {
            int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_err < 0) { perror("open error file"); exit(-1); }
            dup2(fd_err, STDERR_FILENO);
            close(fd_err);
        }
        execvp(argvv[0], argvv);
        perror("execvp error");
        exit(-1);
    }
    return pid;
}

/* Execute a pipeline of commands.
   comandos[] holds the pipeline segments.
   num_cmds is the number of commands in the pipeline.
   Returns 0 on success.
*/
int ejecutar_pipeline(char *comandos[], int num_cmds) {
    int pipefd[2];
    int prev_pipe_read = -1;
    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe error");
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork error");
            return -1;
        } else if (pid == 0) {
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }
            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            char cmdCopy[MAX_LINE];
            strncpy(cmdCopy, comandos[i], MAX_LINE);
            cmdCopy[MAX_LINE - 1] = '\0';
            int num_args = tokenizar_linea(cmdCopy, " \t\n", argvv, MAX_ARGS);
            if (num_args == 0) {
                write(STDERR_FILENO, "Empty command\n", 14);
                exit(-1);
            }
            for (int j = 0; j < num_args; j++) {
                trim_token(argvv[j]);
            }
            procesar_redirecciones(argvv);
            if (i == 0 && filev[0] != NULL) {
                int fd_in = open(filev[0], O_RDONLY);
                if (fd_in < 0) { perror("open input file"); exit(-1); }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (i == num_cmds - 1) {
                if (filev[1] != NULL) {
                    int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) { perror("open output file"); exit(-1); }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                if (filev[2] != NULL) {
                    int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_err < 0) { perror("open error file"); exit(-1); }
                    dup2(fd_err, STDERR_FILENO);
                    close(fd_err);
                }
            }
            execvp(argvv[0], argvv);
            perror("execvp error");
            exit(-1);
        } else {
            pids[i] = pid;
            if (prev_pipe_read != -1)
                close(prev_pipe_read);
            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_pipe_read = pipefd[0];
            }
        }
    }
    if (!background) {
        for (int i = 0; i < num_cmds; i++) {
            waitpid(pids[i], NULL, 0);
        }
    } else {
        char msg[100];
        snprintf(msg, sizeof(msg), "Pipeline running in background, PID: %d\n", pids[num_cmds - 1]);
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    return 0;
}

/* Main: Reads the script file using system calls */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char *usage = "Usage: ./scripter <script_file>\n";
        write(STDERR_FILENO, usage, strlen(usage));
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Error opening the script file");
        return -1;
    }

    char line[MAX_LINE];
    int line_index = 0;
    int line_number = 0;
    char ch;
    ssize_t n;

    while ((n = read(fd, &ch, 1)) > 0) {
        if (ch == '\n') {
            line[line_index] = '\0';
            // Remove any carriage return characters from the line
            for (int i = 0; i < line_index; i++) {
                if (line[i] == '\r') {
                    for (int j = i; j < line_index; j++) {
                        line[j] = line[j+1];
                    }
                    line_index--;
                    i--;
                }
            }
            line_number++;

            if (line_number == 1) {
                if (strncmp(line, "## Script de SSOO", 17) != 0) {
                    write(STDERR_FILENO, "Invalid script format at line 1\n", 32);
                    close(fd);
                    return -1;
                }
                // Skip header line.
            } else {
                if (line_index == 0) {
                    char err[100];
                    snprintf(err, sizeof(err), "Empty command line encountered at line %d\n", line_number);
                    write(STDERR_FILENO, err, strlen(err));
                    close(fd);
                    return -1;
                }
                char *comandos[MAX_COMMANDS];
                int num_cmds = procesar_linea(line, comandos);
                if (num_cmds == 1) {
                    int pid = ejecutar_comando(comandos[0]);
                    if (pid < 0) {
                        close(fd);
                        return -1;
                    }
                    if (!background)
                        waitpid(pid, NULL, 0);
                    else {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "Command running in background, PID: %d\n", pid);
                        write(STDOUT_FILENO, msg, strlen(msg));
                    }
                } else {
                    if (ejecutar_pipeline(comandos, num_cmds) < 0) {
                        close(fd);
                        return -1;
                    }
                }
            }
            line_index = 0;
        } else {
            if (line_index < MAX_LINE - 1)
                line[line_index++] = ch;
        }
    }
    
    if (line_index > 0) {
        line[line_index] = '\0';
        // Remove carriage returns if needed
        for (int i = 0; i < line_index; i++) {
            if (line[i] == '\r') {
                for (int j = i; j < line_index; j++) {
                    line[j] = line[j+1];
                }
                line_index--;
                i--;
            }
        }
        line_number++;
        if (line_number == 1) {
            if (strncmp(line, "## Script de SSOO", 17) != 0) {
                write(STDERR_FILENO, "Invalid script format at line 1\n", 32);
                close(fd);
                return -1;
            }
        } else {
            if (line_index == 0) {
                char err[100];
                snprintf(err, sizeof(err), "Empty command line encountered at line %d\n", line_number);
                write(STDERR_FILENO, err, strlen(err));
                close(fd);
                return -1;
            }
            char *comandos[MAX_COMMANDS];
            int num_cmds = procesar_linea(line, comandos);
            if (num_cmds == 1) {
                int pid = ejecutar_comando(comandos[0]);
                if (pid < 0) {
                    close(fd);
                    return -1;
                }
                if (!background)
                    waitpid(pid, NULL, 0);
                else {
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Command running in background, PID: %d\n", pid);
                    write(STDOUT_FILENO, msg, strlen(msg));
                }
            } else {
                if (ejecutar_pipeline(comandos, num_cmds) < 0) {
                    close(fd);
                    return -1;
                }
            }
        }
    }

    close(fd);
    return 0;
}
