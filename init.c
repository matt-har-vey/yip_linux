#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
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

#define REBOOT_CMD_POWEROFF 0x4321fedc
#define REBOOT_CMD_RESTART 0x1234567

#define CONTROL_TTY "/dev/hvc0"

struct mount_spec {
  const char *source;
  const char *target;
  const char *type;
};

static const struct mount_spec specs[] = {
    {.source = "tmpfs", .target = "/tmp", .type = "tmpfs"},
    {.source = "tmpfs", .target = "/run", .type = "tmpfs"},
    {.source = "proc", .target = "/proc", .type = "proc"},
    {.source = "sysfs", .target = "/sys", .type = "sysfs"},
    {.source = "devpts", .target = "/dev/pts", .type = "devpts"},
    {.source = NULL, .target = NULL, .type = NULL}};

void panic(const char *context) {
  fprintf(stderr, "init panic (context=\"%s\", errno=%d)\n", context, errno);
  perror("init panic");
  while (1) {
    sleep(10);
  }
}

void shutdown();

void startup() {
  if (signal(SIGTERM, shutdown) == SIG_ERR) {
    panic("installing SIGTERM");
  }

  sethostname(HOSTNAME, strlen(HOSTNAME));

  mkdir("/dev/pts", 0755);
  for (const struct mount_spec *spec = specs; spec->target != NULL; ++spec) {
    if (mount(spec->source, spec->target, spec->type, 0, NULL) == -1) {
      fprintf(stderr, "mount(%s, %s, %s, 0, NULL)\n", spec->source,
              spec->target, spec->type);
      panic("mount");
    }
  }
  if (mount(NULL, "/", NULL, MS_REMOUNT, NULL) == -1) {
    panic("remounting /");
  }
  chmod("/tmp", 0777);

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/sh", "sh", "/etc/start_net", NULL);
  } else {
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    if (wstatus != 0) {
      fprintf(stderr, "start_net failed with status %d\n", wstatus);
    }
  }
}

void shutdown() {
  kill(-1, SIGTERM);
  sleep(2);
  kill(-1, SIGQUIT);

  if (umount("/") != 0) {
    panic("umount /");
  }
  for (const struct mount_spec *spec = specs; spec->target != NULL; ++spec) {
    if (umount(spec->target) != 0) {
      panic("umount spec");
    }
  }
  sync();

  if (getenv("YIP_REBOOT") != NULL) {
    reboot(REBOOT_CMD_RESTART);
  } else {
    reboot(REBOOT_CMD_POWEROFF);
  }

  panic("post reboot");
}

void run_console() {
  int tty = open(CONTROL_TTY, O_RDWR);
  if (login_tty(tty) == -1) {
    panic("console login_tty");
    return;
  }
  while (true) {
    pid_t pid = fork();
    if (pid == 0) {
      execl("/usr/bin/login", "login", NULL);
    } else {
      for (pid_t wait_pid = -1; wait_pid != pid; wait_pid = wait(NULL)) {}
    }
  }
  close(tty);
}

int main(int argc, char **argv) {
  startup();

  if (fork() == 0) {
    execl("/usr/sbin/tcp_pty", "tcp_pty", NULL);
    perror("exec tcp_pty");
    return -1;
  }

  run_console();

  panic("post run_console");
  return 0;
}
