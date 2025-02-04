#include "exec.h"
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rsh_loop(void) {
  char *line;
  char **args;
  int status;
  char *quit_cmd = "q";

  do {
    printf("> ");

    line = rsh_read_line();
    if (!strncmp(quit_cmd, line, strlen(quit_cmd))) {
      break;
    }
    args = rsh_split_line(line);
    status = rsh_execute(args);

    free(line);
    free(args);

  } while (status);
}

int main(int argc, char **argv) {

  rsh_loop();

  return 0;
}
