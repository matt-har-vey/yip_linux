#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#define CHAR_LIMIT 2000

int main(int argc, char** argv) {
  if (argc < 2) {
    return -1;
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("open");
    return -1;
  }
  void* buf = malloc(CHAR_LIMIT);
  if (buf == NULL) {
    return -1;
  }
  ssize_t n_read = read(fd, buf, CHAR_LIMIT);
  if (n_read > 0) {
    write(0, buf, n_read);
  }
  free(buf);
  write(0, "\n", 1);
  if (n_read == -1) {
    perror("read");
    return -1;
  }
  return 0;
}
