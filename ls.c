#include <errno.h>
#include <stdio.h>

#include <dirent.h>
#include <sys/types.h>

int main(int argc, char **argv) {
  if (argc <= 1) {
    return -1;
  }
  DIR *dir = opendir(argv[1]);
  if (dir == NULL) {
    perror("opendir");
    return -1;
  }

  for (struct dirent *entry = readdir(dir); entry != NULL;
       entry = readdir(dir)) {
    printf("%s\n", entry->d_name);
  }

  closedir(dir);
  return 0;
}
