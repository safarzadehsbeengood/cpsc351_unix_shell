#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSH_RL_BUFSIZE 1024;      // bufsize for reading a line
#define RSH_TOK_BUFSIZE 64;       // bufsize for tokens
#define RSH_TOK_DELIM " \t\r\n\a" // delimiters for token separation

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
