#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_LIMIT 100

void run_line(char *cmdline) {
  char *argv[CMDLINE_LIMIT];
  memset(argv, 0, sizeof(argv));
  char *arg = cmdline;
  argv[0] = strtok(arg, " ");
  if (argv[0] == 0) {
    return;
  }
  int argc = 1;
  while ((arg = strtok(NULL, " ")) != 0) {
    if (argc >= CMDLINE_LIMIT - 1) {
      printf("too many args\n");
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
  }
}

int main(int argc, char **argv) {
  while (1) {
    printf("$ ");
    char cmdline[CMDLINE_LIMIT];
    memset(cmdline, 0, sizeof(cmdline));
    size_t pos = 0;
    while (1) {
      char c = getchar();
      if (c == EOF) {
        return 0;
      }
      if (c == '\n') {
        cmdline[pos] = 0;
        run_line(cmdline);
        break;
      }
      if (pos >= CMDLINE_LIMIT) {
        printf("command line too long\n");
        break;
      }
      cmdline[pos] = c;
      ++pos;
    }
  }
}
