#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

#include "user_session.h"

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

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: tcp_pty <address> <port>\n");
  }

  // Reaps zombie closed connection handlers.
  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
    perror("signal");
    return -1;
  }

  int sock_fd = resolve_and_bind(argv[1], argv[2]);
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
