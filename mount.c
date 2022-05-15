#include <stdio.h>

#include <sys/mount.h>

int main(int argc, char **argv) {
  char *source = NULL;
  char *target = NULL;
  char *filesystemtype = NULL;
  char *data = NULL;
  if (argc > 1) {
    source = argv[1];
  }
  if (argc > 2) {
    target = argv[2];
  }
  if (argc > 3) {
    filesystemtype = argv[3];
  }
  if (argc > 4) {
    data = argv[4];
  }
  int err = mount(source, target, filesystemtype, 0, data);
  if (err) {
    perror("mount");
  }
  return err;
}
