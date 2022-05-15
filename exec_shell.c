#include "exec_shell.h"

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

void exec_shell(int uid, int gid, const char* home_dir) {
  setsid();
  setgid(gid);
  setuid(uid);
  setenv("HOME", home_dir, 1);
  chdir(home_dir);
  const char bin_sh[] = "/bin/sh";
  const char zsh[] = "/usr/local/bin/zsh";
  struct stat statbuf;
  if (stat(zsh, &statbuf) == 0) {
    execl(zsh, zsh, (char *)NULL);
  } else {
    execl(bin_sh, bin_sh, (char *)NULL);
  }
}
