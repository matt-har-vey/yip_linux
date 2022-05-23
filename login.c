#include <libgen.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

static char *read_line(char *buf, size_t size) {
  memset(buf, 0, size);
  if (fgets(buf, size, stdin) == NULL) {
    return NULL;
  }
  for (ptrdiff_t pos = 0; pos < size; ++pos) {
    const char c = buf[pos];
    if (c == '\r' || c == '\n' || c == 0) {
      buf[pos] = 0;
      break;
    }
  }
  return buf;
}

int main(int argc, char **argv) {
  printf("login: ");
  fflush(stdout);

  char buf[16];
  const char *username = read_line(buf, sizeof(buf));
  if (username == NULL) {
    fprintf(stderr, "read failed\n");
    return -1;
  }

  const struct passwd *pwnam = getpwnam(username);
  if (pwnam == NULL) {
    fprintf(stderr, "unknown login\n");
    return -1;
  }

  if (fchown(0, pwnam->pw_uid, pwnam->pw_gid)) {
    fprintf(stderr, "chown stdin\n");
    return -1;
  }

  if (setgid(pwnam->pw_gid) || setuid(pwnam->pw_uid)) {
    fprintf(stderr, "setuid failed\n");
    return -1;
  }

  setenv("HOME", pwnam->pw_dir, 1);
  chdir(pwnam->pw_dir);
  const char *shell = pwnam->pw_shell;
  char *shell_dup = strdup(shell);
  execl(shell, basename(shell_dup), (char *)NULL);
  return -1;
}
