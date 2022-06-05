use std::os::unix::fs::PermissionsExt;
use std::os::unix::process::CommandExt;
use std::process::Command;
use std::thread::sleep;
use std::time::Duration;

use nix::sys::reboot::{reboot, RebootMode};
use nix::sys::signal as sn;
use nix::sys::signal::kill;
use nix::sys::wait::{waitpid, WaitPidFlag, WaitStatus};
use nix::unistd::Pid;
use signal_hook::consts::signal as sh;
use signal_hook::iterator::Signals;
use sys_mount::{Mount, MountFlags, UnmountDrop, UnmountFlags};

fn mount(
    source: &str,
    target: &str,
) -> Result<UnmountDrop<Mount>, Box<dyn std::error::Error>> {
    Ok(Mount::builder().fstype(source).mount_autodrop(
        source,
        target,
        UnmountFlags::empty(),
    )?)
}

fn reap() {
    while waitpid(Pid::from_raw(-1), Some(WaitPidFlag::WNOHANG))
        != Ok(WaitStatus::StillAlive)
    {}
}

fn run() -> Result<(), Box<dyn std::error::Error>> {
    nix::unistd::sethostname("paddock")?;

    std::fs::create_dir("/dev/pts")?;
    let _mounts = [
        mount("tmpfs", "/tmp")?,
        mount("tmpfs", "/run")?,
        mount("proc", "/proc")?,
        mount("sysfs", "/sys")?,
        mount("devpts", "/dev/pts")?,
        Mount::builder()
            .flags(MountFlags::REMOUNT)
            .mount_autodrop("", "/", UnmountFlags::empty())?,
    ];
    std::fs::set_permissions("/tmp", std::fs::Permissions::from_mode(0o777))?;

    Command::new("/usr/sbin/ifconfig")
        .args(["eth0", "up", "192.168.122.62", "netmask", "255.255.255.0"])
        .spawn()?;
    Command::new("/usr/sbin/tcp_pty").arg0("tcp_pty").spawn()?;

    let mut signals = Signals::new(&[sh::SIGCHLD, sh::SIGTERM])?;
    for signal in signals.into_iter() {
        match signal {
            sh::SIGCHLD => reap(),
            sh::SIGTERM => { break; },
            _ => unreachable!(),
        }
    }

    let _ = kill(Pid::from_raw(-1), sn::SIGTERM);
    sleep(Duration::from_secs(2));
    let _ = kill(Pid::from_raw(-1), sn::SIGQUIT);

    Ok(())
}

fn main() {
    match run() {
        Ok(()) => {
            nix::unistd::sync();
            let reboot_mode = match std::env::var("YIP_REBOOT") {
                Ok(_) => RebootMode::RB_AUTOBOOT,
                Err(_) => RebootMode::RB_POWER_OFF,
            };
            let _ = reboot(reboot_mode);
        },
        Err(e) => {
            eprintln!("init panic: {:?}", e);
        },
    };
    loop {
        sleep(Duration::from_secs(1));
    }
}
