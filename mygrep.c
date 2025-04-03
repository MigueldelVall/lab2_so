#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

const int max_line = 1024;

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Usage: ./mygrep <ruta_fichero> <cadena_busqueda>\n", argv[0]);
        return -1;
    }
    
    
    // Open the file in read-only mode using open()
    int file = open(argv[1], O_RDONLY);
    if (file < 0){
      perror("Error opening the file");
      return -1;
    }
    
    // Declaration of variables
    char line[max_line], buffer[max_line];
    ssize_t bytesRead;
    int found = 0, linePos = 0;
    
    
    //Read the file in blocks of size 'max_line'
    while ((bytesRead = read(file, buffer, max_line)) > 0){
    
      //Processes each character read into the buffer
      for (int i = 0; i<bytesRead; i++){
      
        //When a new line is found, insert '\0' at the end of the string
        if (buffer[i]=='\n'){
          line[linePos] = '\0';
          
          //strstr() is used to search for the text string (passed as the second argument) in the file
          if (strstr(line, argv[2]) != NULL){
             write(1, line, strlen(line));
             write(1, "\n", 1);  
             found = 1;
          }
          
          //Reset the index to start constructing the next line
          linePos = 0; 
        } 
        else if (linePos < max_line -1){
          line[linePos++] = buffer[i];
        }
      }
    }
    
    //If an error occurs during reading an error, an error message is printed
    if(bytesRead < 0){
        perror("Error reading the file");
        if (close(file)<0){
          perror("Error closing the file");
          return -1;
        }
    }
    
    //If the text string is not found, a message is printed
    if (!found){
      write(1, "\"", 1);
      write(1, argv[2], strlen(argv[2]));
      write(1, "\" not found.\n", 14);
    }

    // Close the file, if an error occurs a n error message is printed and return -1
    if (close(file)<0){
      perror("Error closing the file");
      return -1;
    }
    return 0;
}
