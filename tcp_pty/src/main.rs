use nix::unistd::ForkResult;
use std::os::unix::io::FromRawFd;
use std::os::unix::prelude::RawFd;
use tokio::io::AsyncWriteExt;
use tokio::net::TcpListener;

async fn remote_control(
    master: RawFd,
) -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("0.0.0.0:8888").await?;
    let (mut socket, _) = listener.accept().await?;
    let mut ptmx = unsafe { tokio::fs::File::from_raw_fd(master) };
    ptmx.write_all("import os\nos.listdir()\n".as_bytes()).await?;
    tokio::io::copy_bidirectional(&mut socket, &mut ptmx).await?;
    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let pty = unsafe { nix::pty::forkpty(None, None) }?;
    match pty.fork_result {
        ForkResult::Child => {
            return Err(Box::new(
                exec::Command::new("/usr/bin/python3").exec(),
            ));
        }
        ForkResult::Parent { child: _ } => {
            let rt = tokio::runtime::Runtime::new()?;
            return rt.block_on(remote_control(pty.master));
        }
    }
}
