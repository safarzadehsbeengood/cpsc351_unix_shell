#include "exec.h"
#include "parse.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ANSI colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define QUIT_CMD "exit"

void rsh_loop(void) {
  char *line;
  char **args;
  int status;

  // start prompt
  system("clear");
  printf("Welcome to rsh!\nType any system command, \"help\" for help, or \"exit\" to exit!\n");

  do {
    // char* cwd = getcwd(&cwd_buf, PATH_MAX);
    // printf("%s > ", cwd);
    printf(ANSI_COLOR_GREEN "$ " ANSI_COLOR_RESET);

    line = rsh_read_line();
    
    // quit
    if (!strncmp(QUIT_CMD, line, strlen(QUIT_CMD))) {
      break;
    }
    args = rsh_split_line(line);
    status = rsh_execute(args);

    free(line);
    free(args);

  } while (status);
}

int main() {

  rsh_loop();

  return 0;
}
