#include "user_session.h"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 4096

static void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
}

static void wait_to_write(int fd) {
  struct pollfd poll_fd;
  memset(&poll_fd, 0, sizeof(struct pollfd));
  poll_fd.fd = fd;
  poll_fd.events = POLLOUT;
  poll(&poll_fd, 1, -1);
}

static int bridge_fds(int fd_0, int fd_1) {
  const int closed_flags = POLLNVAL | POLLERR | POLLHUP;
  struct pollfd poll_fds[2];
  memset(poll_fds, 0, sizeof(poll_fds));
  poll_fds[0].fd = fd_0;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = fd_1;
  poll_fds[1].events = POLLIN;
  set_nonblocking(fd_0);
  set_nonblocking(fd_1);

  void *buf = malloc(BUF_SIZE);
  if (buf == NULL) {
    return -1;
  }
  while (1) {
    if (poll(poll_fds, 2, -1) == -1) {
      break;
    }
    if ((poll_fds[0].revents & closed_flags) ||
        (poll_fds[1].revents & closed_flags)) {
      break;
    }
    if (poll_fds[0].revents & POLLIN) {
      const ssize_t num_read = read(fd_0, buf, BUF_SIZE);
      if (num_read == 0) {
        break;
      }
      wait_to_write(fd_1);
      if (write(fd_1, buf, num_read) != num_read) {
        break;
      }
    }
    if (poll_fds[1].revents & POLLIN) {
      const ssize_t num_read = read(fd_1, buf, BUF_SIZE);
      if (num_read == 0) {
        break;
      }
      wait_to_write(fd_0);
      if (write(fd_0, buf, num_read) != num_read) {
        break;
      }
    }
  }
  free(buf);
  return 0;
}

static char* read_line(char* buf, size_t size) {
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

static void exec_shell(int uid, int gid, const char *home_dir) {
  setsid();
  setgid(gid);
  setuid(uid);
  setenv("HOME", home_dir, 1);
  chdir(home_dir);
  const char bin_sh[] = "/bin/sh";
  const char zsh[] = "/usr/local/bin/zsh";
  struct stat statbuf;
  if (stat(zsh, &statbuf) == 0) {
    execl(zsh, "zsh", (char *)NULL);
  } else {
    execl(bin_sh, "sh", (char *)NULL);
  }
}

static int login(const char* chown_path) {
  printf("login: ");
  fflush(stdout);

  char buf[16];
  const char* username = read_line(buf, sizeof(buf));
  if (username == NULL) {
    fprintf(stderr, "read failed\n");
    return -1;
  }

  const struct passwd *pwnam = getpwnam(username);
  if (pwnam == NULL) {
    fprintf(stderr, "unknown login\n");
    return -1;
  }

  if (chown_path != NULL) {
    chown(chown_path, pwnam->pw_uid, pwnam->pw_gid);
  }

  if (setgid(pwnam->pw_gid) || setuid(pwnam->pw_uid)) {
    fprintf(stderr, "setuid failed\n");
    return -1;
  }

  exec_shell(pwnam->pw_uid, pwnam->pw_gid, pwnam->pw_dir);
  return -1;
}

int user_session(int conn_fd) {
  if (setsid() == -1) {
    return -1;
  }
  if (isatty(conn_fd)) {
    const pid_t cpid = fork();
    if (cpid == 0) {
      close(0);
      close(1);
      close(2);
      dup(conn_fd);
      dup(conn_fd);
      dup(conn_fd);
      return login(NULL);
    } else {
      int status = -1;
      waitpid(cpid, &status, 0);
    }
  } else {
    int tty_fd = -1;
    char pty_path[16];
    memset(pty_path, 0, sizeof(pty_path));
    const pid_t cpid = forkpty(&tty_fd, pty_path, NULL, NULL);
    if (cpid == -1) {
      return -1;
    } else if (cpid == 0) {
      close(conn_fd);
      return login(pty_path);
    } else {
      bridge_fds(tty_fd, conn_fd);
      close(tty_fd);
    }
  }
  return 0;
}
