#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h> 
#include <stdbool.h>
#include <limits.h> // PATH_MAX

#define HELP_CMD "help"
#define HELP_MSG                                                               \
  "Type any system command, exit to exit, cd [path] to change directory, or "  \
  "mkdir [dirname] to create a new directory\n"
#define QUIT_CMD "exit"

#define RSH_RL_BUFSIZE 1024
#define RSH_TOK_BUFSIZE 64

#define RSH_TOK_DELIM " \t\r\n\a"
#define PIPE_DELIM "|"

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

// stores a single command (no pipes) 
struct {
  char **argv;
} typedef Command;

// stores an instruction (whatever the user typed), can have multiple commands
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
  char *token;
  char **tokens = malloc(sizeof(char *) * bufsize);
  if (!tokens) {
    fprintf(stderr, "rsh_parse_cmd: tokens allocation error");
    exit(EXIT_FAILURE);
  }

  char *saveptr_cmd;  // for strtok
  token = strtok_r(cmd_str, RSH_TOK_DELIM, &saveptr_cmd);
  while (token != NULL) {
    // copy token for storage
    char *token_copy = strdup(token);
    if (!token_copy) {
      fprintf(stderr, "strdup error in rsh_parse_cmd");
      exit(EXIT_FAILURE);
    }
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
    token = strtok_r(NULL, RSH_TOK_DELIM, &saveptr_cmd);
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

// splits a line from the user into an instruction
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

  instr->has_pipe = strchr(line, '|') != NULL; // set has_pipe

  char *line_copy = strdup(line); // copy for strtok
  if (!line_copy) {
    fprintf(stderr, "rsh: line_copy allocation error");
    exit(EXIT_FAILURE);
  }

  char *saveptr_instr; // saveptr for strtok_r
  char *token = strtok_r(line_copy, PIPE_DELIM, &saveptr_instr);
  while (token != NULL) {
    // strip whitespace
    char *start = token;
    while (*start == ' ') start++; // Trim leading spaces

    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ') end--; // Trim trailing spaces
    *(end + 1) = '\0';

    instr->commands[position] = rsh_parse_cmd(start);
    position++;

    if (position >= bufsize) {
      bufsize += RSH_TOK_BUFSIZE;
      instr->commands = realloc(instr->commands, sizeof(Command *) * bufsize);
      if (!instr->commands) {
        fprintf(stderr, "rsh: allocation error");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok_r(NULL, PIPE_DELIM, &saveptr_instr);
  }
  instr->commands[position] = NULL;
  free(line_copy); // Free the duplicated line
  return instr;
}

/* ---------------------------------------------------------------- EXECUTION
 * -----------------------------------------------------------------------------------------
 */

int rsh_launch(Command *cmd) {
  // cd
  if (!strncmp(cmd->argv[0], "cd", strlen("cd"))) {
    if (cmd->argv[1] == NULL) {
      fprintf(stderr, "rsh: expected argument to \"cd\"\n");
    } else {
      if (chdir(cmd->argv[1]) != 0) {
        perror("rsh");
      }
    }
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
  if (pid == 0) {
    // child
    if (execvp(cmd->argv[0], cmd->argv) == -1) {
      perror("rsh");
      exit(EXIT_FAILURE);
    }
  } else if (pid < 0) {
    // error forking
    perror("rsh");
  } else {
    // parent
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

int rsh_execute(Instruction *instr) {
  if (instr->commands[0]->argv[0] == NULL) {
    // empty command
    return 1;
  }

  // no pipe
  if (!instr->has_pipe) {
    return rsh_launch(instr->commands[0]);
  } else {
    // pipe execution
    int num_commands = 0;
    while (instr->commands[num_commands] != NULL) {
      num_commands++;
    }

    int pipefds[num_commands - 1][2]; // need n-1 pipes for n commands

    // create pipes
    for (int i = 0; i < num_commands - 1; i++) {
      if (pipe(pipefds[i]) == -1) {
        perror("pipe");
        fprintf(stderr, "Pipe creation failed for command %d: %s\n", i,
                strerror(errno)); 
        return 1;
      }
    }

    pid_t pids[num_commands];

    for (int i = 0; i < num_commands; i++) {
      pids[i] = fork();

      if (pids[i] == 0) { // child
        
        // redirect input
        if (i > 0) {
          if (dup2(pipefds[i - 1][0], STDIN_FILENO) == -1) {
            perror("dup2 (stdin)");
            fprintf(stderr, "dup2 failed for stdin of command %d: %s\n", i,
                    strerror(errno));
            exit(EXIT_FAILURE);
          }
        }

        // redirect output
        if (i < num_commands - 1) {
          if (dup2(pipefds[i][1], STDOUT_FILENO) == -1) {
            perror("dup2 (stdout)");
            fprintf(stderr, "dup2 failed for stdout of command %d: %s\n", i,
                    strerror(errno));
            exit(EXIT_FAILURE);
          }
        }

        // close all pipe fd
        for (int j = 0; j < num_commands - 1; j++) {
          close(pipefds[j][0]);
          close(pipefds[j][1]);
        }

        // execute the command
        if (execvp(instr->commands[i]->argv[0], instr->commands[i]->argv) ==
            -1) {
          perror("execvp");
          fprintf(stderr, "Exec failed for command %d: %s\n", i,
                  strerror(errno)); // Add error info
          exit(EXIT_FAILURE);
        }
      } else if (pids[i] < 0) {
        // error forking
        perror("fork");
        return 1;
      }
    }

    // parent
    // close all pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++) {
      close(pipefds[i][0]);
      close(pipefds[i][1]);
    }

    // wait for all children
    for (int i = 0; i < num_commands; i++) {
      wait(NULL);
    }
  }

  return 1;
}

/* ------------------------------------------------------ UTILS/TESTING
 * -------------------------------------------------------------- */

void free_cmd(Command *cmd) {
  if (!cmd) return;
  for (int i = 0; cmd->argv[i] != NULL; i++) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
  free(cmd);
}

void free_instr(Instruction *instr) {
  if (!instr) return;
  for (int i = 0; instr->commands[i] != NULL; i++) {
    free_cmd(instr->commands[i]);
  }
  free(instr->commands);
  free(instr);
}

// current working directory
void print_prompt() {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    char *last_slash = strrchr(cwd, '/');
    if (last_slash != NULL) {
      printf(ANSI_COLOR_CYAN "%s " ANSI_COLOR_RESET, last_slash + 1);
    } else {
      printf("%s", cwd);
    }
  } else {
    perror("getcwd() error");
  }
}

/* ---------------------------------------------------------------------------------- MAIN
 * --------------------------------------------------------------------------------- */

void rsh_loop(void) {
  char *line;
  Instruction *instr;
  int status;

  // start prompt
  system("clear");
  printf("Welcome to rsh!\nType any system command, \"help\" for help, or "
         "\"exit\" to exit!\n");

  do {

    print_prompt();
    line = rsh_read_line();

    // quit
    if (!strncmp(QUIT_CMD, line, strlen(QUIT_CMD))) {
      free(line);
      break;
    }
    instr = rsh_parse_instruction(line);
    status = rsh_execute(instr);

    free_instr(instr);
    free(line);

  } while (status);
}

int main() {
  rsh_loop();
  return 0;
}
