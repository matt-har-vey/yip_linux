#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pty.h>
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

int handle_connection(int conn_fd) {
  void* buf = malloc(BUF_SIZE); 
  if (buf == NULL) {
    return -1;
  }

  int target_fd = -1;
  char* pty_name = (char*)malloc(BUF_SIZE);
  memset(pty_name, 0, BUF_SIZE);
  const pid_t cpid = forkpty(&target_fd, pty_name, NULL, NULL);
  if (cpid == -1) {
    free(pty_name);
    perror("forkpty");
    return -1;
  }
  if (cpid == 0) {
    free(pty_name);
    close(conn_fd);
    exec_shell(UID, GID, HOMEDIR);
    fprintf(stderr, "unexpected return from exec_shell\n");
  }
  free(pty_name);

  struct pollfd poll_fds[2];
  memset(poll_fds, 0, sizeof(poll_fds));
  poll_fds[0].fd = conn_fd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = target_fd;
  poll_fds[1].events = POLLIN;

  set_nonblocking(conn_fd);
  set_nonblocking(target_fd);

  const int closed_flags = POLLNVAL | POLLERR | POLLHUP;
  while (1) {
    if (poll(poll_fds, 2, -1) == -1) {
      break;
    }
    if ((poll_fds[0].revents & closed_flags) ||
        (poll_fds[1].revents & closed_flags)) {
      break;
    }
    if (poll_fds[0].revents & POLLIN) {
      const ssize_t num_read = read(conn_fd, buf, BUF_SIZE);
      if (num_read == 0) {
        break;
      }
      wait_to_write(target_fd);
      if (write(target_fd, buf, num_read) != num_read) {
        break;
      }
    }
    if (poll_fds[1].revents & POLLIN) {
      const ssize_t num_read = read(target_fd, buf, BUF_SIZE);
      if (num_read == 0) {
        break;
      }
      wait_to_write(conn_fd);
      if (write(conn_fd, buf, num_read) != num_read) {
        break;
      }
    }
  }

  free(buf);

  close(target_fd);

  return 0;
}

int main(int argc, char** argv) {
  // Reaps zombie closed connection handlers.
  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
      perror("signal");
      return -1;
  }

  int err = 0;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  struct addrinfo *result, *addr;

  err = getaddrinfo(argv[1], argv[2], &hints, &result);
  if (err != 0) {
    fprintf(stderr, "getaddr failed: %s\n", gai_strerror(err));
    return -1;
  }

  int sock_fd = 0;
  for (addr = result; addr != NULL; addr = addr->ai_next) {
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
      continue;
    }
    int reuse_addr = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int));
    if (bind(sock_fd, addr->ai_addr, addr->ai_addrlen) == 0) {
      break;
    }
    close(sock_fd);
  }

  freeaddrinfo(result);
  
  if (addr == NULL) {
    perror("bind");
    return -1;
  }

  const int backlog = 10;
  err = listen(sock_fd, backlog);
  if (err != 0) {
    perror("listen");
    return -1;
  }

  err = 0;
  while (!err) {
    int conn_fd = accept(sock_fd, NULL, NULL);
    if (conn_fd == -1) {
      perror("accept");
      break;
    }

    pid_t conn_pid = fork();
    if (conn_pid == 0) {
      err = handle_connection(conn_fd);
      close(conn_fd);
      break;
    } else {
      close(conn_fd);
    }
  }

  close(sock_fd);
  return err;
}
