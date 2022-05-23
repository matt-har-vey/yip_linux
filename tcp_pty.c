#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pty.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUF_SIZE 4096

static int resolve_and_bind(const char *address, const char *protocol) {
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
  while (true) {
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

int main(int argc, char **argv) {
  const char kHost[] = "0.0.0.0";
  const char kPort[] = "8888";

  int sock_fd = resolve_and_bind(kHost, kPort);
  if (sock_fd == -1) {
    fprintf(stderr, "bind failed\n");
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
      sleep(1);
      continue;
    }

    if (fork() == 0) {
      int ptmx = -1, pts = -1;
      char pty_path[16];
      memset(pty_path, 0, sizeof(pty_path));
      if (openpty(&ptmx, &pts, pty_path, NULL, NULL) == -1) {
        perror("openpty");
        return -1;
      }
      const pid_t pid = fork();
      if (pid == 0) {
        close(ptmx);
        close(conn_fd);
        close(sock_fd);
        if (login_tty(pts) == -1) {
          perror("login_tty");
          return -1;
        }
        execl("/usr/bin/login", "login", NULL);
      } else {
        close(pts);
        if (pid > 0) {
          bridge_fds(ptmx, conn_fd);
        }
        close(conn_fd);
        close(sock_fd);
        return 0;
      }
    } else {
      close(conn_fd);
    }
  }

  close(sock_fd);
  return 0;
}
