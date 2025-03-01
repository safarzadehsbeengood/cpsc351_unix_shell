#include <errno.h>
#include <limits.h> // PATH_MAX
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
  Instruction
    - Command (if more than one, pipe)
      - Arguments
      - Redirect Stream (input)
      - Redirect Stream (output)
      - Execute (should this be executed)
    - has_pipe (contains a pipe?)
    - Execute
*/

// stores a single command (no pipes)
struct {
  char **argv; // array of arguments example (ls -l => argv[0] = ls, argv[1] =
               // -l, argv[2] = NULL)
  char *output_file; // Stores filename for output redirection
  char *input_file;
  bool append; // True for >> (append), False for > (overwrite)
  bool execute;
} typedef Command;

// stores an instruction (whatever the user typed), can have multiple commands
struct {
  Command **
      commands; // holds multiple commands: example: for ls | grep .c ->
                // commands[0] = {"ls", NULL},commands[1] = {"grep", ".c", NULL}
  bool has_pipe; // true if user command has a pipe
  bool execute;
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
  char *output_file = NULL;
  char *input_file = NULL;
  bool append = false;
  if (!tokens) {
    fprintf(stderr, "rsh_parse_cmd: tokens allocation error");
    exit(EXIT_FAILURE);
  }

  char *saveptr_cmd; // for strtok_r to save where it is

  token = strtok_r(cmd_str, RSH_TOK_DELIM,
                   &saveptr_cmd); // splits string into tokens for parsing

  while (token != NULL) {

    if (strcmp(token, "<") == 0) {
      token = strtok_r(NULL, RSH_TOK_DELIM, &saveptr_cmd);
      if (token == NULL) {
        fprintf(stderr, "rsh: syntax error near input redirect\n");
        exit(EXIT_FAILURE);
      }

      input_file = strdup(token);                          // save input file
      token = strtok_r(NULL, RSH_TOK_DELIM, &saveptr_cmd); // get next token

      // only output redirect or pipe should be after <
      continue;
    }

    // finds > or >> and stores the filename to write or append to
    if (strcmp(token, ">") == 0 ||
        strcmp(token, ">>") == 0) {        // if ">" or ">>" is found in token
      append = (strcmp(token, ">>") == 0); // True if >>, else false
      token = strtok_r(NULL, RSH_TOK_DELIM, &saveptr_cmd);
      if (token == NULL) {
        fprintf(stderr, "rsh: syntax error near redirection\n");
        exit(EXIT_FAILURE);
      }
      output_file = strdup(token); // set input after > as output_file
      continue; // next token for this command should be null
    }

    // copy token for storage
    char *token_copy = strdup(token); // makes a copy of the token
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
    token = strtok_r(NULL, RSH_TOK_DELIM,
                     &saveptr_cmd); // incroments the next thing in cmd_str
                                    // based off RSH_TOK_DELIM
  }
  tokens[position] = NULL;
  Command *cmd = malloc(sizeof(Command));

  if (!cmd) {
    fprintf(stderr, "rsh: cmd allocation error");
    exit(EXIT_FAILURE);
  }

  cmd->argv = tokens;
  cmd->output_file = output_file;
  cmd->input_file = input_file;
  cmd->append = append;
  cmd->execute = true;

  // handle echo
  if (!strcmp(tokens[position - 1], "ECHO") ||
      !strcmp(tokens[position - 1], "PIPE") ||
      !strcmp(tokens[position - 1], "IO")) {
    cmd->execute = false;
    free(cmd->argv[position - 1]);
    cmd->argv[position - 1] = NULL;
  }
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

  instr->execute = true;
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
    while (*start == ' ')
      start++; // Trim leading spaces

    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ')
      end--; // Trim trailing spaces
    *(end + 1) = '\0';

    // parse this command
    instr->commands[position] = rsh_parse_cmd(start);
    // if any of the commands are set to not execute, do not execute the
    // instruction
    if (!instr->commands[position]->execute)
      instr->execute = false;
    position++;

    // realloc if needed
    if (position >= bufsize) {
      bufsize += RSH_TOK_BUFSIZE;
      instr->commands = realloc(instr->commands, sizeof(Command *) * bufsize);
      if (!instr->commands) {
        fprintf(stderr, "rsh: allocation error");
        exit(EXIT_FAILURE);
      }
    }
    // get the next command
    token = strtok_r(NULL, PIPE_DELIM, &saveptr_instr);
  }
  // set end to null
  instr->commands[position] = NULL;
  free(line_copy); // free the duplicated line
  return instr;
}

/* ---------------------------------------------------------------- EXECUTION
 * -----------------------------------------------------------------------------------------
 */

int rsh_launch(Command *cmd) {
  // cd handle
  if (!strncmp(cmd->argv[0], "cd", strlen("cd"))) {
    if (cmd->argv[1] == NULL) {
      fprintf(stderr, "rsh: expected argument to \"cd\"\n");
    } else {
      if (chdir(cmd->argv[1]) != 0) {
        printf("cd: No such file or directory %s\n", cmd->argv[1]);
      }
    }
    return 1;
  }

  // help handle
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
    FILE *fd_out = NULL, *fd_in = NULL;

    if (cmd->input_file) {
      fd_in = fopen(cmd->input_file, "r");
      if (!fd_in) {
        fprintf(stderr, "rsh: cannot open input file %s\n", cmd->input_file);
        exit(EXIT_FAILURE);
      }
      dup2(fileno(fd_in), STDIN_FILENO);
      fclose(fd_in);
    }

    if (cmd->output_file) { // checks if there is an output file
      if (cmd->append) {
        fd_out = fopen(cmd->output_file, "a"); // appends
      } else {
        fd_out = fopen(cmd->output_file, "w"); // writes
      }
      if (!fd_out) {
        fprintf(stderr, "rsh: cannot open output file %s\n", cmd->output_file);
        exit(EXIT_FAILURE);
      }
      dup2(fileno(fd_out), STDOUT_FILENO); // makes it so it prints to a file
      fclose(fd_out);
    }

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

// creates a pipeline if needed and executes an instruction
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
    // get number of commands
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

        // Handle input redirection for the first command
        if (i == 0 && instr->commands[i]->input_file) {
          FILE *fd_in = fopen(instr->commands[i]->input_file, "r");
          if (!fd_in) {
            fprintf(stderr, "rsh: cannot open input file %s\n",
                    instr->commands[i]->input_file);
            exit(EXIT_FAILURE);
          }
          dup2(fileno(fd_in), STDIN_FILENO);
          fclose(fd_in);
        }

        // Handle output redirection for the last command
        if (instr->commands[i]->output_file) {
          FILE *fd;
          if (instr->commands[i]->append) {
            fd = fopen(instr->commands[i]->output_file, "a"); // appends
          } else {
            fd = fopen(instr->commands[i]->output_file, "w"); // writes
          }
          if (!fd) {
            fprintf(stderr, "rsh: cannot open output file %s\n",
                    instr->commands[i]->output_file);
            exit(EXIT_FAILURE);
          }
          dup2(fileno(fd), STDOUT_FILENO);
          fclose(fd);
        }

        // execute the command
        if (execvp(instr->commands[i]->argv[0], instr->commands[i]->argv) ==
            -1) {
          perror("execvp");
          fprintf(stderr, "exec failed for command %d: %s\n", i,
                  strerror(errno));
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

// free a command
void free_cmd(Command *cmd) {
  if (!cmd)
    return;
  for (int i = 0; cmd->argv[i] != NULL; i++) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
  free(cmd->output_file);
  free(cmd->input_file);
  free(cmd);
}

// free an instruction
void free_instr(Instruction *instr) {
  if (!instr)
    return;
  for (int i = 0; instr->commands[i] != NULL; i++) {
    free_cmd(instr->commands[i]);
  }
  free(instr->commands);
  free(instr);
}

// print current working directory (not absolute)
void print_prompt() {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    char *last_slash = strrchr(cwd, '/');
    if (last_slash != NULL) {
      printf(ANSI_COLOR_CYAN "%s " ANSI_COLOR_RESET "> ", last_slash + 1);
    } else {
      printf("%s", cwd);
    }
  } else {
    perror("getcwd() error");
  }
}

void print_cmd(Command *cmd) {
  for (int i = 0; cmd->argv[i] != NULL; i++) {
    if (i == 0) {
      fprintf(stderr, ANSI_COLOR_GREEN "%s " ANSI_COLOR_RESET, cmd->argv[i]);
    } else {
      fprintf(stderr, "%s ", cmd->argv[i]);
    }
  }
  if (cmd->input_file)
    fprintf(stderr, ANSI_COLOR_RED "LT " ANSI_COLOR_RESET "%s ", cmd->input_file);
  if (cmd->output_file)
    fprintf(stderr, ANSI_COLOR_RED "GT " ANSI_COLOR_RESET "%s ", cmd->output_file);
}

void print_instr(Instruction *instr) {
  for (int i = 0; instr->commands[i] != NULL; i++) {
    if (i != 0)
      fprintf(stderr, ANSI_COLOR_CYAN " PIPE " ANSI_COLOR_RESET);
    print_cmd(instr->commands[i]);
  }
  fprintf(stderr, "\n");
}

/* ----------------------------------------------------------------------------------
 * MAIN
 * ---------------------------------------------------------------------------------
 */

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
    fprintf(stderr, "Execute -> %d\n", instr->execute);
    if (instr->execute) {
      status = rsh_execute(instr);
    } else {
      print_instr(instr);
      status = 1;
    }

    free_instr(instr);
    free(line);

  } while (status);
}

int main() {
  rsh_loop();
  return 0;
}
