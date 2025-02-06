#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HELP_CMD "help"
#define HELP_MSG "Type any system command, exit to exit, cd [path] to change directory, or mkdir [dirname] to create a new directory\n"
#define QUIT_CMD "exit"

#define RSH_RL_BUFSIZE 1024;      // bufsize for reading a line
#define RSH_TOK_BUFSIZE 64;       // bufsize for tokens
#define RSH_TOK_DELIM " \t\r\n\a" // delimiters for token separation

// ANSI colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// func to read a line from the user during main loop
char *rsh_read_line(void) {
  int bufsize = RSH_RL_BUFSIZE;                  // init bufsize
  int pos = 0;                                   // character position
  char *buffer = malloc(sizeof(char) * bufsize); // allocate buffer
  int c;                                         // current character

  // failed to allocate buffer
  if (!buffer) {
    fprintf(stderr, "rsh: allocation error");
    exit(EXIT_FAILURE);
  }

  // copy line into buffer
  while (1) {
    c = getchar();
    if (c == EOF || c == '\n') {
      buffer[pos] = '\0';
      return buffer;
    } else {
      buffer[pos] = c;
    }
    pos++;

    // if the buffer is too small, rellocate twice the current size
    if (pos >= bufsize) {
      bufsize += RSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      // reallocation failure if buffer is null
      if (!buffer) {
        fprintf(stderr, "rsh: reallocation error");
        exit(EXIT_FAILURE);
      }
    }
  }
}

// func to split a line for the args
char **rsh_split_line(char *line) {
  int bufsize = RSH_TOK_BUFSIZE int position = 0;
  char **tokens = malloc(sizeof(char *) * bufsize); // allocate token buffer
  char *token;                                      // current token (word/arg)

  // failed to allocate
  if (!tokens) {
    fprintf(stderr, "rsh: allocation error");
    exit(EXIT_FAILURE);
  }

  // use strtok to separate tokens by delimiters
  token = strtok(line, RSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += RSH_TOK_BUFSIZE;
      tokens = realloc(tokens, sizeof(char *) * bufsize);
      if (!tokens) {
        fprintf(stderr, "rsh: allocation error");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, RSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

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
    printf(ANSI_COLOR_GREEN "$ > " ANSI_COLOR_RESET);

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
