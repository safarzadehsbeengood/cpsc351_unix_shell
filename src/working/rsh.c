#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HELP_CMD "help"
#define HELP_MSG                                                               \
  "Type any system command, exit to exit, cd [path] to change directory, or "  \
  "mkdir [dirname] to create a new directory\n"
#define QUIT_CMD "exit"

#define RSH_RL_BUFSIZE 1024;      // bufsize for reading a line
#define RSH_TOK_BUFSIZE 64;       // bufsize for tokens
#define RSH_TOK_DELIM " \t\r\n\a" // delimiters for token separation
#define PIPE_DELIM "|"            // delimiter for pipe separation

// ANSI colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

/* ---------------------------------------------------------------- PARSING
  ----------------------------------------------------------------------------------------------
  Instruction
    - Command (if more than one, pipe)
      - Arguments
    - Redirect Stream (NULL by default)
    - has_pipe
*/

// command struct
struct {
  char **argv;
} typedef Command;

// Stores an instruction (whatever the user typed)
struct {
  Command **commands;
  bool has_pipe;
  char *redirect_stream;
} typedef Instruction;

// func to read a line from the user during main loop
char *rsh_read_line(void) {
  char *line = NULL;
  size_t bufsize = 0;

  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);
    } else {
      perror("readline");
      exit(EXIT_FAILURE);
    }
  }

  return line;
}

// func to parse a command with arguments
Command *rsh_parse_cmd(char *cmd_str) {
  int bufsize = RSH_TOK_BUFSIZE;
  int position = 0;
  char *token;                                      // current token (word/arg)
  char **tokens = malloc(sizeof(char *) * bufsize); // allocate tokens
  if (!tokens) {
    fprintf(stderr, "rsh_parse_cmd: tokens allocation error");
    exit(EXIT_FAILURE);
  }

  // use strtok to separate tokens by delimiters
  token = strtok(cmd_str, RSH_TOK_DELIM);
  while (token != NULL) {
    char *token_copy = strdup(token);
    tokens[position] = token_copy;
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
  Command *cmd = malloc(sizeof(Command));
  if (!cmd) {
    fprintf(stderr, "rsh: cmd allocation error");
    exit(EXIT_FAILURE);
  }
  cmd->argv = tokens;
  return cmd;
}

// Splits a line into an instruction
Instruction *rsh_parse_instruction(char *line) {
  int bufsize = RSH_TOK_BUFSIZE;
  int position = 0;

  Instruction *instr = malloc(sizeof(Instruction));
  if (!instr) {
    fprintf(stderr, "rsh: instr allocation error");
    exit(EXIT_FAILURE);
  }
  instr->commands = malloc(sizeof(Command *) * bufsize);
  if (!instr->commands) {
    fprintf(stderr, "rsh: instr commands allocation error");
    exit(EXIT_FAILURE);
  }
  char *token = strtok(line, PIPE_DELIM);
  while (token != NULL) {
    char *token_copy = strdup(token);
    if (!token_copy) {
      fprintf(stderr, "rsh: token_copy allocation error");
      exit(EXIT_FAILURE);
    }
    instr->commands[position] = rsh_parse_cmd(token_copy);
    position++;
    free(token_copy);

    if (position >= bufsize) {
      bufsize += RSH_TOK_BUFSIZE;
      instr->commands = realloc(instr->commands, sizeof(Command *) * bufsize);
      if (!instr->commands) {
        fprintf(stderr, "rsh: allocation error");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, PIPE_DELIM);
  }
  instr->commands[position] = NULL;
  return instr;
}

/* ---------------------------------------------------------------- EXECUTION
 * -----------------------------------------------------------------------------------------
 */
int rsh_launch(int in, int out, Command *cmd) {

  // cd
  if (!strncmp(cmd->argv[0], "cd", strlen("cd"))) {
    if (chdir(cmd->argv[1]))
      printf("cd: no such file or directory \"%s\"\n", cmd->argv[1]);
    return 1;
  }

  // help
  if (!strncmp(HELP_CMD, cmd->argv[0], strlen(HELP_CMD))) {
    printf(HELP_MSG);
    return 1;
  }

  // if not help or cd, then a sys cmd
  pid_t pid;
  int status;
  pid = fork();

  if (pid == 0) { // child
    if (execvp(cmd->argv[0], cmd->argv) == -1) {
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

int rsh_execute(Instruction *instr) {
  if (instr->commands[0]->argv[0] == NULL) { // empty command
    return 1;
  }
  return rsh_launch(STDIN_FILENO, STDOUT_FILENO, instr->commands[0]);
}

/* ------------------------------------------------------ UTILS/TESTING
 * -------------------------------------------------------------- */

void print_instruction(Instruction *instr) {
  for (int i = 0; instr->commands[i] != NULL; i++) {
    for (int j = 0; instr->commands[i]->argv[j] != NULL; j++) {
      char *arg = instr->commands[i]->argv[j];
      printf("instr %d cmd %d arg: %s\n", i, j, arg);
    }
  }
}

void print_cmd(Command *cmd) {
  for (int i = 0; cmd->argv[i] != NULL; i++) {
    printf("arg %d: %s\n", i, cmd->argv[i]);
  }
}

void free_cmd(Command *cmd) {
  if (!cmd) return;
  for (int i = 0; cmd->argv[i] != NULL; i++) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
  free(cmd);
}

void free_instr(Instruction *instr) {
  for (int i = 0; instr->commands[i] != NULL; i++) {
    free_cmd(instr->commands[i]);
  }
  free(instr->commands);
  free(instr);
}

void test_cmd() {
  char *cmd = strdup("ls -l -a");
  if (!cmd) {
    fprintf(stderr, "strdup error");
    exit(EXIT_FAILURE);
  }
  Command *res = rsh_parse_cmd(cmd);
  print_cmd(res);
  free(cmd);
  free_cmd(res);
}

void test_instr() {
  // test rsh_split_command
  char *test_line = strdup("ls -l -a | grep test");
  if (!test_line) {
    fprintf(stderr, "strdup error");
    exit(EXIT_FAILURE);
  }

  printf("test line: %s\n", test_line);
  Instruction *instr = rsh_parse_instruction(test_line);
  print_instruction(instr);
  free_instr(instr);
  free(test_line);
}

/* ---------------------------------------------------------------------------------- MAIN --------------------------------------------------------------------------------- */

void rsh_loop(void) {
  char *line;
  Instruction *instr;
  int status;

  // start prompt
  system("clear");
  printf("Welcome to rsh!\nType any system command, \"help\" for help, or "
         "\"exit\" to exit!\n");

  do {
    // char* cwd = getcwd(&cwd_buf, PATH_MAX);
    // printf("%s > ", cwd);
    printf(ANSI_COLOR_GREEN "$ > " ANSI_COLOR_RESET);

    line = rsh_read_line();

    // quit
    if (!strncmp(QUIT_CMD, line, strlen(QUIT_CMD))) {
      free(line);
      break;
    }
    instr = rsh_parse_instruction(line);
    print_instruction(instr);
    status = rsh_execute(instr);

    free_instr(instr);
    free(line);

  } while (status);
}

int main() {
  // test_cmd();
  // test_instr();
  rsh_loop();
  return 0;
}
