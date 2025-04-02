# NO TOCAR / NOT MODIFIED ME ##
CC=gcc
FLAGS=-Wno-implicit-function-declaration
CFLAGS=-I.
###############################

# MODIFIED ME ##
# Object files
OBJ_SCRIPTER = scripter.o
OBJ_MYGREP = mygrep.o

# Default target: Compile both programs
all: scripter mygrep

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $< $(CFLAGS)

# Compile the 'scripter' executable
scripter: $(OBJ_SCRIPTER)
	$(CC) $(FLAGS) -o $@ $^ $(CFLAGS)

# Compile the 'mygrep' executable
mygrep: $(OBJ_MYGREP)
	$(CC) $(FLAGS) -o $@ $^ $(CFLAGS)

# Clean up compiled files
clean:
	rm -f $(OBJ_SCRIPTER) $(OBJ_MYGREP) scripter mygrep
