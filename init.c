#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>

#define REBOOT_CMD_HALT 0xcdef0123
#define REBOOT_CMD_POWEROFF 0x4321fedc

int main(int argc, char **argv) {
  if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
    perror("mount proc");
  }
  if (mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) {
    perror("mount sys");
  }
  if (mount("tmpfs", "/run", "tmpfs", 0, NULL) == 1) {
    perror("mount run");
  }

  pid_t pid = fork();
  if (pid == 0) {
    const char sh[] = "/bin/sh";
    execl(sh, sh, (char *)NULL);
  } else {
    int wstatus = 0;
    wait(&wstatus);
    sync();
    if (umount("/") != 0) {
      perror("umount");
    }
    if (getenv("YIP_NOREBOOT") == NULL) {
      if (reboot(REBOOT_CMD_POWEROFF) != 0) {
        perror("reboot");
      }
    }
  }
  return -1;
}
