#include <stdio.h>
#include <unistd.h>

#include <linux/reboot.h>

#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  pid_t pid = fork();
  if (pid == 0) {
    const char sh[] = "/bin/sh";
    execl(sh, sh, (char *)NULL);
  } else {
    int wstatus = 0;
    wait(&wstatus);
    sync();
    if (reboot(LINUX_REBOOT_CMD_POWER_OFF) != 0) {
      perror("reboot");
    }
    printf("reboot returned.\nwaiting forever\n");
    while (1) {
      sleep(3600);
    }
  }
}
