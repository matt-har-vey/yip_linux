use tokio::net::TcpListener;
use tokio::net::TcpStream;
use tokio::process::Command;

async fn session(socket: TcpStream) {
    match Command::new("python3").spawn() {
        Ok(mut child) => {
            println!("successfully spawned {:?}", child);
            match child.wait().await {
                Ok(exit_status) => println!("exited with code {:?}", exit_status.code()),
                Err(e) => eprintln!("wait error: {:?}", e),
            }
        },
        Err(e) => {
            eprintln!("spawn error: {:?}", e);
        }
    }
}

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8888").await?;

    loop {
        let (socket, _) = listener.accept().await?;
        tokio::spawn(async move {
            session(socket).await
        });
    }
}
