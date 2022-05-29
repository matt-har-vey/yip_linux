use nix::errno::Errno;
use nix::unistd::ForkResult;
use std::ffi::CString;
use std::ffi::NulError;
use std::os::unix::io::FromRawFd;
use std::os::unix::prelude::RawFd;
use tokio::net::TcpListener;
use tokio::net::TcpStream;

fn forkpty() -> Result<nix::pty::ForkptyResult, nix::Error> {
    unsafe { nix::pty::forkpty(None, None) }
}

fn set_nonblocking(fd: RawFd) -> Result<(), nix::Error> {
    let bits = nix::fcntl::fcntl(fd, nix::fcntl::FcntlArg::F_GETFL)?;
    let mut flags = nix::fcntl::OFlag::from_bits_truncate(bits);
    flags.insert(nix::fcntl::OFlag::O_NONBLOCK);
    nix::fcntl::fcntl(fd, nix::fcntl::FcntlArg::F_SETFL(flags))?;
    Ok(())
}

#[derive(Debug)]
enum ExecError {
    Argument(NulError),
    Call(Errno),
}

impl From<NulError> for ExecError {
    fn from(err: NulError) -> ExecError {
        ExecError::Argument(err)
    }
}

impl From<Errno> for ExecError {
    fn from(err: Errno) -> ExecError {
        ExecError::Call(err)
    }
}

fn exec_login() -> Result<(), ExecError> {
    nix::unistd::execv(&CString::new("/usr/bin/login")?, &[CString::new("login")?])?;
    Ok(())
}

async fn process(mut socket: TcpStream) {
    match forkpty() {
        Ok(pty) => match pty.fork_result {
            ForkResult::Child => {
                if let Err(e) = exec_login() {
                    println!("exec_login error: {:?}", e);
                }
                println!("process() child returning");
            }
            ForkResult::Parent { child: _ } => {
                // if let Err(e) = set_nonblocking(pty.master) {
                //     println!("set_nonblocking error: {:?}", e);
                //     return;
                // }
                let mut ptmx = unsafe { tokio::fs::File::from_raw_fd(pty.master) };
                match tokio::io::copy_bidirectional(&mut ptmx, &mut socket).await {
                    Ok((sent, received)) => {
                        println!("{} bytes sent; {} bytes received", sent, received);
                    }
                    Err(e) => {
                        println!("channel error: {:?}", e);
                        return;
                    }
                }
            }
        },
        Err(e) => println!("forkpty failed; err = {:?}", e),
    }
}

#[tokio::main(flavor="current_thread")]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("0.0.0.0:8889").await?;

    loop {
        let (socket, _) = listener.accept().await?;

        tokio::spawn(async move {
            process(socket).await;
        });
    }
}
