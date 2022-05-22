#include "user_session.h"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "exec_shell.h"

#define BUF_SIZE 1024

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

int user_session(int conn_fd) {
  if (setsid() == -1) {
    close(conn_fd);
    return -1;
  }

  FILE *conn = fdopen(conn_fd, "r+");
  if (conn == NULL) {
    close(conn_fd);
    return -1;
  }
  setbuf(conn, NULL);

  fprintf(conn, "login: ");

  char username[100];
  memset(username, 0, sizeof(username));
  for (size_t pos = 0;; ++pos) {
    if (pos == sizeof(username)) {
      fprintf(conn, "username overflow\n");
      fclose(conn);
      return -1;
    }
    const char c = getc(conn);
    if (c == '\r' || c == '\n' || c == 0) {
      username[pos] = 0;
      fprintf(conn, "\r\n");
      break;
    }
    putc(c, conn);
    username[pos] = c;
  }

  const struct passwd *pwnam = getpwnam(username);
  if (pwnam == NULL || setgid(pwnam->pw_gid) || setuid(pwnam->pw_uid)) {
    fprintf(conn, "unknown login\r\n");
    fclose(conn);
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
      exec_shell(pwnam->pw_uid, pwnam->pw_gid, pwnam->pw_dir);
      return -1;
    } else {
      int status = -1;
      waitpid(cpid, &status, 0);
    }
  } else {
    int tty_fd = -1;
    const pid_t cpid = forkpty(&tty_fd, NULL, NULL, NULL);
    if (cpid == -1) {
      fprintf(conn, "forkpty failed\r\n");
    } else if (cpid == 0) {
      fclose(conn);
      exec_shell(pwnam->pw_uid, pwnam->pw_gid, pwnam->pw_dir);
      return -1;
    } else {
      bridge_fds(tty_fd, conn_fd);
      close(tty_fd);
    }
  }

  fclose(conn);
  return 0;
}
