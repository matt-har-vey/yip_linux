#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "exec_shell.h"

#define HOSTNAME "paddock"

#define REBOOT_CMD_HALT 0xcdef0123
#define REBOOT_CMD_POWEROFF 0x4321fedc

#define LOGIN_TTY "/dev/ttyS0"

void panic(const char *context) {
  fprintf(stderr, "init panic (context=\"%s\", errno=%d)", context, errno);
  while (1) {
    sleep(10);
  }
}

void serial_login() {
  prctl(PR_SET_NAME, (unsigned long)"serial_login", 0, 0, 0);
  while (1) {
    pid_t pid = fork();
    if (pid == 0) {
      int fd = open(LOGIN_TTY, O_RDWR);
      login_tty(fd);
      exec_shell(100, 1000, "/home/bellatrix");
    } else {
      int wstatus = 0;
      waitpid(pid, &wstatus, 0);
    }
  }
}

void system_control() {
  pid_t pid = fork();
  if (pid == 0) {
    exec_shell(0, 0, "/root");
  } else {
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    sync();
    if (umount("/") != 0) {
      panic("umount /");
    }
    if (getenv("YIP_NOREBOOT") == NULL) {
      if (reboot(REBOOT_CMD_POWEROFF) != 0) {
        panic("reboot");
      }
    }
  }
}

int main(int argc, char **argv) {
  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
    panic("installing SIGCHLD");
  }

  sethostname(HOSTNAME, strlen(HOSTNAME));

  if (mount("/dev/sda1", "/", "ext4", MS_REMOUNT, NULL) == -1) {
    panic("mount /");
  }
  if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
    panic("mount /proc");
  }
  if (mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) {
    panic("mount /sys");
  }
  if (mount("tmpfs", "/run", "tmpfs", 0, NULL) == -1) {
    panic("mount /run");
  }
  mkdir("/dev/pts", 0755);
  if (mount("devpts", "/dev/pts", "devpts", 0, NULL) == -1) {
    panic("mount /dev/pts");
  }

  pid_t pid = -1;
  pid = fork();
  if (pid == 0) {
    serial_login();
  }

  pid = fork();
  if (pid == 0) {
    execl("/bin/sh", "/bin/sh", "/etc/start_net", NULL);
  } else {
    int wstatus;
    waitpid(pid, &wstatus, 0);
    if (wstatus != 0) {
      fprintf(stderr, "start_net failed with status %d\n", wstatus);
    }
  }

  pid = fork();
  if (pid == 0) {
    execl("/sbin/shelld", "/sbin/shelld", "0.0.0.0", "8888", NULL);
  }

  system_control();

  panic("unexpected exit");
  return -1;
}
