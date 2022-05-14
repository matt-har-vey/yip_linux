#include <fcntl.h>
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

#define HOSTNAME "paddock"

#define REBOOT_CMD_HALT 0xcdef0123
#define REBOOT_CMD_POWEROFF 0x4321fedc

#define LOGIN_TTY "/dev/ttyS0"

void exec_shell(int uid, int gid, const char* home_dir) {
  setsid();
  setgid(gid);
  setuid(uid);
  setenv("HOME", home_dir, 1);
  chdir(home_dir);
  const char bin_sh[] = "/bin/sh";
  const char zsh[] = "/usr/local/bin/zsh";
  struct stat statbuf;
  if (stat(zsh, &statbuf) == 0) {
    execl(zsh, zsh, (char *)NULL);
  } else {
    execl(bin_sh, bin_sh, (char *)NULL);
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
}

int main(int argc, char **argv) {
  sethostname(HOSTNAME, strlen(HOSTNAME));

  if (mount("/dev/sda1", "/", "ext4", MS_REMOUNT, NULL) == -1) {
    perror("mount /");
  }
  if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
    perror("mount /proc");
  }
  if (mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) {
    perror("mount /sys");
  }
  if (mount("tmpfs", "/run", "tmpfs", 0, NULL) == -1) {
    perror("mount /run");
  }

  pid_t pid = fork();
  if (pid == 0) {
    serial_login();
  } else {
    system_control();
  }

  // init should not exit
  return -1;
}
