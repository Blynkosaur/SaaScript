#include "../include/bytecode/chunk.h"
#include "../include/common.h"
#include "../include/debug.h"
#include "../include/vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
repl() { // REPL read eval print loop when you just run >>python and puts u
  // in that weird ass environment
  char line[1024];
  while (1) {
    printf(">>> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");

      break;
    }
    if (strncmp(line, "quit()", 6) == 0) {

      fprintf(stdout, "Exited\n");
      break;
    }
    interpret(line);
  }
}

static char *readFile(char *path) {
  FILE *file = fopen(path, "rb");
  // check for errors

  if (file == NULL) {
    fprintf(stderr, "could not open file \"%s\". \n", path);
    exit(74);
  }

  // go to the end of the file
  fseek(file, 0L, SEEK_END);
  long int file_size =
      ftell(file); // int of the position of the file as a byte count
  rewind(file);    // goes back to the beginning
  /*
  alternative is fseek(file, 0L, SEEK_SET) which also goes back to the beginning
  */

  char *buffer = malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), file_size, file);
  if (bytesRead < file_size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[file_size] = '\0';
  fclose(file);
  return buffer;
}
static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

int main(int argc, const char *argv[]) {
  initVM();
  // writes the constant's index to the byte chunk
  /*
  So after everything ts looks like
  OP_CONSTANT 0 OP_RETURN
  CST_ARRAY = {1.2} --> THE 1.2 IS AT INDEX 0 SO THE OPERATION CAN KNOW WHAT
  INDEX TO ACCESS NEXT
  */
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "usage: bryte [path]\n");
    exit(64);
  }

  freeVM();

  return 0;
}
