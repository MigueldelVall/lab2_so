#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

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
int procesar_linea(char *linea, char *comandos[]) {
    // Reinitialize background to 0
    background = 0; 
    
    //Reinitialize the command array
    for(int i = 0; i<max_commands; i++){
      comandos[i] = NULL;
    }
    
    int num_comandos = tokenizar_linea(linea, "|", comandos, max_commands);
    
    //Check if background is indicated
    if (strchr(comandos[num_comandos - 1], '&')) {
        background = 1;
        char *pos = strchr(comandos[num_comandos - 1], '&'); 
        //remove character 
        *pos = '\0';
    }

    //Finish processing
    for (int i = 0; i < num_comandos; i++) {
        int args_count = tokenizar_linea(comandos[i], " \t\n", argvv, max_args);
        procesar_redirecciones(argvv);

        /********* This piece of code prints the command, args, redirections and background. **********/
        /*********************** REMOVE BEFORE THE SUBMISSION *****************************************/
        /*********************** IMPLEMENT YOUR CODE FOR PROCESSES MANAGEMENT HERE ********************/
        /*printf("Comando = %s\n", argvv[0]);
        for(int arg = 1; arg < max_args; arg++)
            if(argvv[arg] != NULL)
                printf("Args = %s\n", argvv[arg]); 
                
        printf("Background = %d\n", background);
        if(filev[0] != NULL)
            printf("Redir [IN] = %s\n", filev[0]);
        if(filev[1] != NULL)
            printf("Redir [OUT] = %s\n", filev[1]);
        if(filev[2] != NULL)
            printf("Redir [ERR] = %s\n", filev[2]);*/
        /**********************************************************************************************/
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

int ejecutar_comando(char *command) {
    char *args[max_args];
    char *cmdCopy = strdup(command);
    if (!cmdCopy) {
        perror("strdup error");
        return -1;
    }
    tokenizar_linea(cmdCopy, " \t\n", args, max_args);
    procesar_redirecciones(args);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        free(cmdCopy);
        return -1;
    }
    if (pid == 0) { /* Child process */
        /* Handle input redirection if specified */
        if (filev[0] != NULL) {
            int file_in = open(filev[0], O_RDONLY);
            if (file_in < 0) {
                perror("open input redirection error");
                exit(-1);
            }
            if (dup2(file_in, 0) < 0) {
                perror("dup2 input redirection error");
                exit(-1);
            }
            close(file_in);
        }
        /* Handle output redirection if specified */
        if (filev[1] != NULL) {
            int file_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_out < 0) {
                perror("open output redirection error");
                exit(-1);
            }
            if (dup2(file_out, 1) < 0) {
                perror("dup2 output redirection error");
                exit(-1);
            }
            close(file_out);
        }
        /* Handle error redirection if specified */
        if (filev[2] != NULL) {
            int file_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_err < 0) {
                perror("open error redirection error");
                exit(-1);
            }
            if (dup2(file_err, 2) < 0) {
                perror("dup2 error redirection error");
                exit(-1);
            }
            close(file_err);
        }
        
        if(strcmp(args[0], "mygrep")==0){
          args[0] = "./mygrep";
        }
        
        execvp(args[0], args);
        perror("execvp error");
        exit(-1);
    }
    free(cmdCopy);
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



int main(int argc, char *argv[]) {
    if (argc != 2) {
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
          
          //
          lineNumber++;
          
          if (lineNumber == 1){
            if (strncmp(line, "## Script de SSOO", 17) != 0){
              perror("Invalid script format at line 1.");
                close(file);
                return -1;
            }
          }else{
            if(lineIndex == 0){
              perror("Empty line encountered");
              close(file);
              return -1;
            } // final if(line_index == 0)
            
            char *commands[max_commands];
            int numCmds = procesar_linea(line, commands); //devuelve el numero de comandos que hay en la linea 
            // Once you know how many commands (or segments) there are: 
                //For a single command, fork, set up redirections if any, execute the command with execvp(), and wait if necessary
                //For a pipeline, set up the required pipes, fork for each command, use dup2() to connect their inputs/outputs through the pioes, execute each command, and then wait for the pipeline to complete (or handle background execution appropriately).
            if(numCmds == 1){
              int pid = ejecutar_comando(commands[0]);
              
              if(pid < 0){
                close(file);
                return -1;
              } 
              //si el comando no es en background, el padre espera a que el hijo finalice
              if(!background){
                waitpid(pid, NULL, 0);
              }
              // si es en background, se imprime el PID del proceso.
              else{
                printf("%d", pid);
                
              }
              
              
            }
            else{
              if(ejecutar_pipelines(commands, numCmds) < 0){
                close(file);
                return -1;
              }
            }
          }
          
          lineIndex = 0; 
        } 
        else if (lineIndex < max_line -1){
          line[lineIndex++] = buffer[i];
          
          
        }
      }
    }

    if (close(file)<0){
      perror("Error closing the file");
      return -1;
    }
    close(file);
    return 0;
}
