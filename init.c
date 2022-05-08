#include <stdio.h>
#include <unistd.h>

#include <linux/reboot.h>

#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  mount("proc", "/proc", "proc", 0, NULL);
  mount("sysfs", "/sys", "sysfs", 0, NULL);
  mount("tmpfs", "/run", "tmpfs", 0, NULL);

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
      sleep(5);
    }
    if (reboot(LINUX_REBOOT_CMD_POWER_OFF) != 0) {
      perror("reboot");
    }
  }
  return -1;
}
