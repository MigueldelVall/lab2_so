#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

const int max_line = 1024;

int main(int argc, char ** argv) {
    if (argc != 3) {
        perror("Usage: ./mygrep <ruta_fichero> <cadena_busqueda>\n");
        return -1;
    }
    
    
    //Open file using fopen()
    int file = open(argv[1], O_RDONLY);
    if (file < 0){
      perror("Error opening the file");
      return -1;
    }
    
    char line[max_line];
    char buffer[max_line];
    ssize_t bytesRead;
    int found = 0, lineIndex = 0;
    

    while ((bytesRead = read(file, buffer, max_line)) > 0){
      for (int i = 0; i<bytesRead; i++){
      
        if (buffer[i]=='\n'){
        
          line[lineIndex] = '\0';
          
          if (strstr(line, argv[2]) != NULL){
             write(1, line, strlen(line));
             write(1, "\n", 1);  
             found = 1;
          }
          lineIndex = 0; //Resert index for next time
        } 
        else if (lineIndex < max_line -1){
          line[lineIndex++] = buffer[i];
          
        }
      }

    }

    if(bytesRead < 0){
        perror("Error reading the file");
        if (close(file)<0){
          perror("Error closing the file");
          return -1;
        }
    }
    
    if (!found){
      write(1, "\"", 1);
      write(1, argv[2], strlen(argv[2]));
      write(1, "\" not found.\n", 14);
    }

    
    if (close(file)<0){
      perror("Error closing the file");
      return -1;
    }
    
    
    return 0;
}
