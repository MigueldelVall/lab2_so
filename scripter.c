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
   Sets filev[] for redirections and removes the redirection tokens from args */
void procesar_redirecciones(char *args[]) {
    // Reset redirection file pointers
    filev[0] = filev[1] = filev[2] = NULL;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            filev[0] = args[i+1];
            args[i] = NULL;
            args[i+1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            filev[1] = args[i+1];
            args[i] = NULL;
            args[i+1] = NULL;
        } else if (strcmp(args[i], "!>") == 0) {
            filev[2] = args[i+1];
            args[i] = NULL;
            args[i+1] = NULL;
        }
    }
}

/* Process a command line.
   This function splits the line into pipeline segments (commands separated by "|")
   and returns the number of commands.
   The background flag is set if the last command ends with "&".
   For each command, the tokens are stored in the global argvv array.
*/
int procesar_linea(char *linea, char *comandos[]) {
    int num_comandos = tokenizar_linea(linea, "|", comandos, MAX_COMMANDS);
    
    // Check for background flag in the last command
    background = 0;
    char *amp = strchr(comandos[num_comandos - 1], '&');
    if (amp != NULL) {
        background = 1;
        *amp = '\0';  // Remove the '&' character
    }
    
    return num_comandos;
}

/* Execute a single command (after tokenizing it into argvv and processing redirections) */
int ejecutar_comando(char *cmd) {
    // Duplicate command string because strtok modifies it
    char cmdCopy[MAX_LINE];
    strncpy(cmdCopy, cmd, MAX_LINE);
    cmdCopy[MAX_LINE - 1] = '\0';

    // Tokenize the command into arguments using whitespace as delimiter
    int num_args = tokenizar_linea(cmdCopy, " \t\n", argvv, MAX_ARGS);
    if (num_args == 0) {
        return -1; // no command found
    }
    // Process redirections in the tokens
    procesar_redirecciones(argvv);

    // Fork and execute
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        return -1;
    } else if (pid == 0) {
        // Child process: setup redirections if any
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
        // Execute the command
        execvp(argvv[0], argvv);
        perror("execvp error");
        exit(-1);
    }
    return pid;
}

/* Execute a pipeline of commands.
   comandos[] holds the pipeline segments (each command string).
   num_cmds is the number of commands in the pipeline.
   Returns 0 on success.
*/
int ejecutar_pipeline(char *comandos[], int num_cmds) {
    int pipefd[2];
    int prev_pipe_read = -1;  // fd to be used for input for current command
    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < num_cmds; i++) {
        // If not the last command, create a new pipe.
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
            // Child process

            // If there is a previous pipe, duplicate its read end to STDIN.
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }
            // If not the last command, duplicate current pipe's write end to STDOUT.
            if (i < num_cmds - 1) {
                close(pipefd[0]);  // Close unused read end
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            // Process the current command:
            // Duplicate the command string as it will be tokenized.
            char cmdCopy[MAX_LINE];
            strncpy(cmdCopy, comandos[i], MAX_LINE);
            cmdCopy[MAX_LINE - 1] = '\0';
            int num_args = tokenizar_linea(cmdCopy, " \t\n", argvv, MAX_ARGS);
            if (num_args == 0) {
                fprintf(stderr, "Empty command\n");
                exit(-1);
            }
            procesar_redirecciones(argvv);
            // For the first command, if there is input redirection, process it.
            if (i == 0 && filev[0] != NULL) {
                int fd_in = open(filev[0], O_RDONLY);
                if (fd_in < 0) { perror("open input file"); exit(-1); }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            // For the last command, if there is output/error redirection, process them.
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
            // Execute the command
            execvp(argvv[0], argvv);
            perror("execvp error");
            exit(-1);
        } else {
            // Parent process: store the pid.
            pids[i] = pid;
            // Close the previous pipe read end if exists
            if (prev_pipe_read != -1)
                close(prev_pipe_read);
            // For current command, if a new pipe was created, set prev_pipe_read for next iteration.
            if (i < num_cmds - 1) {
                close(pipefd[1]);  // Parent does not need the write end
                prev_pipe_read = pipefd[0];
            }
        }
    }

    // For foreground execution, wait for all children.
    if (!background) {
        for (int i = 0; i < num_cmds; i++) {
            waitpid(pids[i], NULL, 0);
        }
    } else {
        // For background, print the pid of the last command in the pipeline.
        printf("Pipeline running in background, PID: %d\n", pids[num_cmds - 1]);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <script_file>\n", argv[0]);
        return -1;
    }
    
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening the script file");
        return -1;
    }
    
    char line[MAX_LINE];
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
    
        // Remove the newline character
        //line[strcspn(line, "\n")] = '\0';
        
        
        // If empty line, error out.
        if (strlen(line) == 0) {
            fprintf(stderr, "Empty command line encountered at line %d\n", line_number);
            fclose(file);
            return -1;
        }
        int len=strlen(line);
        line[len]= '\0';
        printf("%s",line);
        // Check the first line format.
        if (line_number == 1) {
            if (strncmp(line, "## Script de SSOO", 17) != 0) {
                perror("Invalid script format at line 1\n");
                fclose(file);
                return -1;
            }
            // Skip processing the header line.
            return;
        }
        
        // Process the command line.
        char *comandos[MAX_COMMANDS];
        int num_cmds = procesar_linea(line, comandos);
        
        // If only one command in the pipeline, execute it simply.
        if (num_cmds == 1) {
            int pid = ejecutar_comando(comandos[0]);
            if (pid < 0) {
                fclose(file);
                return -1;
            }
            if (!background)
                waitpid(pid, NULL, 0);
            else
                printf("Command running in background, PID: %d\n", pid);
        } else {
            // Execute multiple commands connected via pipes.
            if (ejecutar_pipeline(comandos, num_cmds) < 0) {
                fclose(file);
                return -1;
            }
        }
    }
    
    fclose(file);
    return 0;
}
