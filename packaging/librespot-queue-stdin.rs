use librespot::connect::{LoadContextOptions, LoadRequest, LoadRequestOptions, Options, Spirc};
use std::io::{BufRead, Read};
use tokio::sync::mpsc;

pub fn start() -> Option<mpsc::Receiver<String>> {
    if std::env::var("TONAL_QUEUE_STDIN").as_deref() != Ok("1") {
        return None;
    }
    let (tx, rx) = mpsc::channel(4);
    std::thread::spawn(move || {
        let mut input = std::io::stdin().lock();
        let mut line = String::new();
        // One header and at most 500 Spotify track URIs.
        while input.by_ref().take(500 * 37 + 64).read_line(&mut line).unwrap_or(0) != 0 {
            if !line.ends_with('\n') {
                break;
            }
            if tx.blocking_send(std::mem::take(&mut line)).is_err() {
                break;
            }
        }
    });
    eprintln!("tonal-queue-ready-v2");
    Some(rx)
}

pub fn handle(line: &str, spirc: Option<&Spirc>) {
    if let Some(tracks) = line.trim_end().strip_prefix("playqueue|") {
        let uris: Vec<String> = tracks.split(',').take(501).map(str::to_owned).collect();
        if !uris.is_empty() && uris.len() <= 500 &&
            uris.iter().all(|uri| uri.strip_prefix("spotify:track:").is_some_and(|id|
                id.len() == 22 && id.bytes().all(|byte| byte.is_ascii_alphanumeric()))) {
            if let Some(spirc) = spirc {
                let _ = spirc.load(LoadRequest::from_tracks(uris, LoadRequestOptions {
                    start_playing: true,
                    context_options: Some(LoadContextOptions::Options(Options::default())),
                    ..Default::default()
                }));
            }
        }
        return;
    }
    let Some((serial, tracks)) = line.trim_end().strip_prefix("editqueue|")
        .and_then(|fields| fields.split_once('|')) else {return;};
    let Ok(serial) = serial.parse::<u64>() else {return;};
    let result = spirc.and_then(|spirc| {
        spirc.edit_queue(tracks.split(',').take(501).map(str::to_owned).collect()).ok()
    });
    tokio::spawn(async move {
        let accepted = match result {
            Some(reply) => reply.await.unwrap_or(false),
            None => false,
        };
        let status = if accepted {"ok"} else {"rejected"};
        eprintln!("tonal-queue-result|{serial}|{status}");
    });
}
