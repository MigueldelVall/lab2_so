#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Usage: %s <ruta_fichero> <cadena_busqueda>\n", argv[0]);
        return -1;
    }
    
    
    //Open file using fopen()
    FILE* file = fopen(argv[1], "r");
    if (!file){
      perror("Error opening the file");
      return -1;
    }
    
    char line[1024];
    int found = 0;
    
    //Read wach line with fgets()
    while (fgets(line, sizeof(line), file)){
      //Check for the substring using strstr()
      if (strstr(line, argv[2])){
        printf("%s", line);  //If found, print the line, if not, print <string> not found.
        found = 1;
      }
    }
    


    if (!found){
      printf("%s not found. \n", argv[2]);
    }
    
    
    
    if (fclose(file)==EOF){
      perror("Error closing the file");
      return -1;
    };  
    
    
    return 0;
}
