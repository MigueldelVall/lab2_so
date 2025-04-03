#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

/* CONST VARS */
const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 //stdin, stdout, stderr
#define max_args 15

/* VARS TO BE USED FOR THE STUDENTS */
char * argvv[max_args];
char * filev[max_redirections];
int background = 0; 

/**
 * This function splits a char* line into different tokens based on a given character
 * @return Number of tokens 
 */
int tokenizar_linea(char *linea, char *delim, char *tokens[], int max_tokens) {
    int i = 0;
    char *token = strtok(linea, delim);
    while (token != NULL && i < max_tokens - 1) {
        tokens[i++] = token;
        token = strtok(NULL, delim);
    }
    tokens[i] = NULL;
    return i;
}

/**
 * This function processes the command line to evaluate if there are redirections. 
 * If any redirection is detected, the destination file is indicated in filev[i] array.
 * filev[0] for STDIN
 * filev[1] for STDOUT
 * filev[2] for STDERR
 */
void procesar_redirecciones(char *args[]) {
    //initialization for every command
    filev[0] = NULL;
    filev[1] = NULL;
    filev[2] = NULL;
    //Store the pointer to the filename if needed.
    //args[i] set to NULL once redirection is processed
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            filev[0] = args[i+1];
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            filev[1] = args[i+1];
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], "!>") == 0) {
            filev[2] = args[i+1];
            args[i] = NULL; 
            args[i + 1] = NULL;
        }
    }
}

/**
 * This function processes the input command line and returns in global variables: 
 * argvv -- command an args as argv 
 * filev -- files for redirections. NULL value means no redirection. 
 * background -- 0 means foreground; 1 background.
 */
int procesar_linea(char *linea) {
    // Reinicializamos las variables globales
    background = 0;
    for (int i = 0; i < max_args; i++) {
        argvv[i] = NULL;
    }
    for (int i = 0; i < max_redirections; i++) {
        filev[i] = NULL;
    }
    
    // Declaramos un arreglo local para separar la línea en comandos (por pipe)
    char *comandos[max_commands];
    for (int i = 0; i < max_commands; i++) {
        comandos[i] = NULL;
    }
    
    int num_comandos = tokenizar_linea(linea, "|", comandos, max_commands);
    
    // Verificamos si el último comando tiene "&" y lo eliminamos
    if (num_comandos > 0 && strchr(comandos[num_comandos - 1], '&') != NULL) {
        background = 1;
        char *pos = strchr(comandos[num_comandos - 1], '&');
        *pos = '\0';
    }
    
    // Para el caso de un único comando, llenamos la variable global argvv
    // usando la tokenización completa de la subcadena y procesamos redirecciones.
    if (num_comandos == 1) {
        tokenizar_linea(comandos[0], " \t\n", argvv, max_args);
        procesar_redirecciones(argvv);
        // Opcional: quitar comillas alrededor de argumentos
        for (int i = 0; argvv[i] != NULL; i++) {
            int len = strlen(argvv[i]);
            if (len >= 2 && argvv[i][0] == '"' && argvv[i][len - 1] == '"') {
                argvv[i][len - 1] = '\0';
                argvv[i] = argvv[i] + 1;
            }
        }
    }
    
    return num_comandos;
}

/* ====================================================
   Function: ejecutar_comando
   Executes a single command (with possible redirections).
   It tokenizes the command string into arguments, processes any
   redirections, then forks a child to execute the command using execvp.
   Returns the PID of the child (or -1 on error).
   ==================================================== */

int ejecutar_comando(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        return -1;
    }
    if (pid == 0) {  /* Proceso hijo */
        // Redirección de entrada
        if (filev[0] != NULL) {
            int fd_in = open(filev[0], O_RDONLY);
            if (fd_in < 0) {
                perror("open input redirection error");
                exit(-1);
            }
            if (dup2(fd_in, 0) < 0) {
                perror("dup2 input redirection error");
                exit(-1);
            }
            close(fd_in);
        }
        // Redirección de salida
        if (filev[1] != NULL) {
            int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("open output redirection error");
                exit(-1);
            }
            if (dup2(fd_out, 1) < 0) {
                perror("dup2 output redirection error");
                exit(-1);
            }
            close(fd_out);
        }
        // Redirección de error
        if (filev[2] != NULL) {
            int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_err < 0) {
                perror("open error redirection error");
                exit(-1);
            }
            if (dup2(fd_err, 2) < 0) {
                perror("dup2 error redirection error");
                exit(-1);
            }
            close(fd_err);
        }
        
        // Si se ejecuta "mygrep", ajustar el path
        if (strcmp(argvv[0], "mygrep") == 0) {
            argvv[0] = "./mygrep";
        }
        
        execvp(argvv[0], argvv);
        perror("execvp error");
        exit(-1);
    }
    return pid;
}


/* ====================================================
   Function: ejecutar_pipeline
   Executes multiple commands connected by pipes.
   'comandos' is an array of command strings, and num_cmds is the number
   of piped commands. For each command, the function tokenizes the command,
   processes redirections, and then sets up the pipeline using dup2.
   It waits for all child processes to finish.
   Returns 0 on success or -1 on error.
   ==================================================== */
int ejecutar_pipelines(char *comandos[], int num_cmds) {
    int pipefds[2 * (num_cmds - 1)];
    pid_t pids[num_cmds];
    
    /* Create pipes */
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe error");
            return -1;
        }
    }
    
    for (int i = 0; i < num_cmds; i++) {
        char *args[max_args];
        char *cmd_copy = strdup(comandos[i]);
        if (!cmd_copy) {
            perror("strdup error");
            return -1;
        }
        tokenizar_linea(cmd_copy, " \t\n", args, max_args);
        procesar_redirecciones(args);
        
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork error");
            free(cmd_copy);
            return -1;
        } else if (pids[i] == 0) {  /* Child process */
            /* For commands after the first, set input from previous pipe */
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], 0) < 0) {
                    perror("dup2 input error");
                    exit(-1);
                }
            } else { /* First command: handle input redirection if any */
                if (filev[0] != NULL) {
                    int fd_in = open(filev[0], O_RDONLY);
                    if (fd_in < 0) {
                        perror("open input redirection error");
                        exit(-1);
                    }
                    if (dup2(fd_in, 0) < 0) {
                        perror("dup2 input redirection error");
                        exit(-1);
                    }
                    close(fd_in);
                }
            }
            /* For commands before the last, set output to next pipe */
            if (i < num_cmds - 1) {
                if (dup2(pipefds[i * 2 + 1], 1) < 0) {
                    perror("dup2 output error");
                    exit(-1);
                }
            } else { /* Last command: handle output and error redirection if any */
                if (filev[1] != NULL) {
                    int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        perror("open output redirection error");
                        exit(-1);
                    }
                    if (dup2(fd_out, 1) < 0) {
                        perror("dup2 output redirection error");
                        exit(-1);
                    }
                    close(fd_out);
                }
                if (filev[2] != NULL) {
                    int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_err < 0) {
                        perror("open error redirection error");
                        exit(-1);
                    }
                    if (dup2(fd_err, 2) < 0) {
                        perror("dup2 error redirection error");
                        exit(-1);
                    }
                    close(fd_err);
                }
            }
            /* Close all pipe file descriptors in child */
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }
            execvp(args[0], args);
            perror("execvp error");
            exit(-1);
        }
        free(cmd_copy);
    }
    
    /* Parent process closes all pipe file descriptors */
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }
    
    /* Parent waits for all child processes */
    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }
    return 0;
}




int ejecutar_linea(char *linea) {
    char *commands[max_commands];
    for (int i = 0; i < max_commands; i++) {
        commands[i] = NULL;
    }
    int numCmds = tokenizar_linea(linea, "|", commands, max_commands);
    if (numCmds == 1) {
        int pid = ejecutar_comando();
        if (pid < 0) {
            return -1;
        }
        if (!background) {
            waitpid(pid, NULL, 0);
        } else {
            printf("%d", pid);
        }
    } else {
        if (ejecutar_pipelines(commands, numCmds) < 0) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
	      errno = EINVAL;
        perror("Usage: ./scripter <script_file>\n");
        return -1;
    }

    int file = open(argv[1], O_RDONLY);
    if (file < 0) {
        perror("Error opening the script file");
        return -1;
    }

    char line[max_line];
    char buffer[max_line];
    ssize_t bytesRead;
    int lineNumber=0, lineIndex = 0;
    
    
    while ((bytesRead = read(file, buffer, max_line)) > 0){
      for (int i = 0; i<bytesRead; i++){
      
        if (buffer[i]=='\n'){
        
          line[lineIndex] = '\0';
          
          
          lineNumber++;
          
          if (lineNumber == 1){
            if (strncmp(line, "## Script de SSOO", 17) != 0){
              errno = EINVAL;
              perror("Invalid script format at line 1.");
                close(file);
                return -1;
            }
          }else{
            if(lineIndex == 0){
              errno = EINVAL;  // Clear any previous error to avoid misleading perror messages
              perror("Empty line encountered");
              close(file);
              return -1;
            } // final if(line_index == 0)
            
            int numCmds = procesar_linea(line); //devuelve el numero de comandos que hay en la linea 
            if (ejecutar_linea(line) < 0){
              close(file);
              return -1;
            }
          }
          
          lineIndex = 0; 
        } 
        else if (lineIndex < max_line -1){
          line[lineIndex++] = buffer[i];
        
        }
      }
    }
    
    if (lineIndex > 0) {
    line[lineIndex] = '\0';
    lineNumber++;
    if (lineNumber == 1) {
      if (strncmp(line, "## Script de SSOO", 17) != 0) {
        errno = 0;  // Clear any previous error to avoid misleading perror messages
        perror("Invalid script format at line 1.");
        close(file);
        return -1;
        }
    } else {
       
          if (ejecutar_linea(line) < 0){
            close(file);
            return -1;
          }
    }
}
    
    
    close(file);
    return 0;
}
