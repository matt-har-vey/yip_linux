#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "exec_shell.h"

#define BUF_SIZE 1024

#define UID 100
#define GID 1000
#define HOMEDIR "/home/bellatrix"

int resolve_and_bind(const char *address, const char *protocol) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  struct addrinfo *addr = NULL;

  int err = getaddrinfo(address, protocol, &hints, &addr);
  if (err != 0) {
    fprintf(stderr, "getaddr failed: %s\n", gai_strerror(err));
    return -1;
  }

  int sock_fd = -1;
  for (struct addrinfo *iter_addr = addr; iter_addr != NULL;
       iter_addr = iter_addr->ai_next) {
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
      continue;
    }
    int reuse_addr = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int));
    if (bind(sock_fd, iter_addr->ai_addr, iter_addr->ai_addrlen) == 0) {
      break;
    }
    close(sock_fd);
    sock_fd = -1;
  }

  freeaddrinfo(addr);
  addr = NULL;

  return sock_fd;
}

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
}

void wait_to_write(int fd) {
  struct pollfd poll_fd;
  memset(&poll_fd, 0, sizeof(struct pollfd));
  poll_fd.fd = fd;
  poll_fd.events = POLLOUT;
  poll(&poll_fd, 1, -1);
}

int bridge_fds(int fd_0, int fd_1) {
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
  const char kDevNull[] = "/dev/null";
  freopen(kDevNull, "r", stdin);
  freopen(kDevNull, "a", stdout);
  freopen(kDevNull, "a", stderr);

  if (setsid() == -1) {
    return -1;
  }

  FILE* conn = fdopen(conn_fd, "r+");
  if (conn == NULL) {
    return -1;
  }
  setbuf(conn, NULL);

  fprintf(conn, "login: ");

  char username[100];
  memset(username, 0, sizeof(username));
  for (size_t pos = 0;; ++pos) {
    if (pos == sizeof(username)) {
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

  const struct passwd* pwnam = getpwnam(username);
  if (pwnam == NULL || setgid(pwnam->pw_gid) || setuid(pwnam->pw_uid)) {
    fprintf(conn, "unknown login\r\n");
    fclose(conn);
    return -1;
  }

  int pty = -1;
  const pid_t cpid = forkpty(&pty, NULL, NULL, NULL);
  if (cpid == -1) {
    fprintf(conn, "failed to get tty\r\n");
    fclose(conn);
    return -1;
  }
  if (cpid == 0) {
    fclose(conn);
    exec_shell(pwnam->pw_uid, pwnam->pw_gid, pwnam->pw_dir);
    return -1;
  }

  bridge_fds(pty, conn_fd);

  fclose(conn);
  close(pty);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: shelld <address> <port>\n");
  }

  // Reaps zombie closed connection handlers.
  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
    perror("signal");
    return -1;
  }

  int sock_fd = resolve_and_bind(argv[1], argv[2]);
  if (sock_fd == -1) {
    fprintf(stderr, "bind failed");
    return -1;
  }

  const int backlog = 10;
  int err = listen(sock_fd, backlog);
  if (err != 0) {
    perror("listen");
    close(sock_fd);
    return err;
  }

  while (true) {
    int conn_fd = accept(sock_fd, NULL, NULL);
    if (conn_fd == -1) {
      perror("accept");
      continue;
    }
    if (fork() == 0) {
      close(sock_fd);
      user_session(conn_fd);
      close(conn_fd);
      break;
    }
    close(conn_fd);
  }

  close(sock_fd);
  return 0;
}
