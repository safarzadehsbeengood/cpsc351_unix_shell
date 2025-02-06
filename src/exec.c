#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HELP_CMD "help"
#define HELP_MSG "Type any system command, exit to exit, cd [path] to change directory, or mkdir [dirname] to create a new directory\n"

int rsh_launch(char **args) {

  // cd
  if (!strncmp(args[0], "cd", strlen("cd"))) {
    if (chdir(args[1])) printf("cd: no such file or directory \"%s\"\n", args[1]);
    return 1;
  }

  // help
  if (!strncmp(HELP_CMD, args[0], strlen(HELP_CMD))) {
    printf(HELP_MSG);
    return 1;
  }

  // if not help or cd, then a sys cmd
  pid_t pid;
  int status;
  pid = fork();

  if (pid == 0) { // child
    if (execvp(args[0], args) == -1) {
      perror("rsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) { // fork error
    perror("rsh");
  } else { // parent
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}

int rsh_execute(char **args) {
  if (args[0] == NULL) { // empty command
    return 1;
  }
  return rsh_launch(args);
}
