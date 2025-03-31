#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Usage: %s <ruta_fichero> <cadena_busqueda>\n", argv[0]);
        return -1;
    }
    
    
    //Open file using fopen()
    int file = open(argv[1], O_RDONLY);
    if (file < 0){
      perror("Error opening the file");
      return -1;
    }
    
    char line[BUFFER_SIZE+1];
    int found = 0;
    
    //
    while ((bytes_read = read(file, line, sizeof(line)) > 0){
        line[bytes_read] = '\0'
      //Check for the substring using strstr()
      if (strstr(line, argv[2])){
        printf("%s", line);  //If found, print the line, if not, print <string> not found.
        found = 1;
      }
    }

    if(bytes_read == -1){
        perror("Error reading the file");
    }
    
    if (!found){
      printf("%s not found. \n", argv[2]);
    }

    
    if (close(file)<0){
      perror("Error closing the file");
      return -1;
    }
    
    
    return 0;
}
