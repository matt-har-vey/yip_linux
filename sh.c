#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_LIMIT 100

#define READ_LINE_EOF -1
#define READ_LINE_OVERFLOW -2

int run_line(char *cmdline) {
  char *argv[CMDLINE_LIMIT];
  memset(argv, 0, sizeof(argv));
  char *arg = cmdline;
  argv[0] = strtok(arg, " ");
  if (argv[0] == 0) {
    return 0;
  }
  int argc = 1;
  while ((arg = strtok(NULL, " ")) != 0) {
    if (argc >= CMDLINE_LIMIT - 1) {
      fprintf(stderr, "too many args\n");
    }
    argv[argc] = arg;
    ++argc;
  }
  argv[argc] = 0;
  pid_t pid = fork();
  if (pid == 0) {
    if (execvp(argv[0], argv) == -1) {
      perror(argv[0]);
      exit(-1);
    }
  } else {
    int wstatus = 0;
    wait(&wstatus);
    return wstatus;
  }
}

size_t read_line(FILE *file, char *buf, size_t limit) {
  size_t pos = 0;
  while (1) {
    char c = getc(file);
    if (c == EOF) {
      return READ_LINE_EOF;
    }
    if (pos >= limit) {
      return READ_LINE_OVERFLOW;
    }
    if (c == '\n') {
      buf[pos] = 0;
      return pos;
    }
    buf[pos] = c;
    ++pos;
  }
}

int run_session(FILE *input, bool interactive) {
  int status = 0;
  while (1) {
    if (interactive) {
      printf("$ ");
      fflush(stdout);
    }
    char cmdline[CMDLINE_LIMIT];
    memset(cmdline, 0, sizeof(cmdline));
    size_t num_read = read_line(input, cmdline, CMDLINE_LIMIT);
    if (num_read == READ_LINE_EOF) {
      return status;
    }
    if (num_read == READ_LINE_OVERFLOW) {
      fprintf(stderr, "command line too long\n");
      continue;
    }
    if (cmdline[0] == '#') {
      continue;
    }
    status = run_line(cmdline);
  }
}

int run_script(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("fopen");
    return errno;
  }
  int status = run_session(file, false);
  fclose(file);
  return status;
}

int main(int argc, char **argv) {
  if (argc > 1) {
    return run_script(argv[1]);
  }
  return run_session(stdin, true);
}
