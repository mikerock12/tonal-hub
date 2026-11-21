"""Prepare the pinned upstream CLI with Tonal's queue-only command and track marker."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import tarfile
from urllib.request import urlretrieve


ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "out" / "librespot-windows"
BUILD.mkdir(parents=True, exist_ok=True)
VERSION = "0.8.0"
PINS = {
    "librespot": "030c5e98cc06f20283b5948ae23bdac1de0dec58dee2c5ec4c8dafc7aaa796ab",
    "librespot-connect": "4b2a21e9e19b906c6676b1a515fc5529d5414f69049f5a167dfb2ea3206e5a27",
    "librespot-playback": "88258620bf3e6808ea1fadd11639648d77c06280b9f5a4c9d14ea79f6f998af6",
}


def source(name):
    filename = f"{name}-{VERSION}.crate"
    cargo = Path(os.environ.get("CARGO_HOME") or Path.home() / ".cargo")
    archive = next((cargo / "registry" / "cache").glob(f"*/{filename}"), BUILD / filename)
    if not archive.exists():
        urlretrieve(f"https://static.crates.io/crates/{name}/{filename}", archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != PINS[name]:
        raise RuntimeError(f"Wrong SHA-256: {archive}")
    with tarfile.open(archive, "r:gz") as package:
        package.extractall(BUILD, filter="data")
    return BUILD / f"{name}-{VERSION}"


def replace(text, before, after):
    if text.count(before) != 1:
        raise RuntimeError(f"Unexpected upstream source for {before!r}")
    return text.replace(before, after)


cli = source("librespot")
connect = source("librespot-connect")
playback = source("librespot-playback")
shutil.copyfile(ROOT / "android/native/vendor/librespot-connect/src/queue_edit.rs",
                connect / "src/queue_edit.rs")
shutil.copyfile(ROOT / "packaging/librespot-queue-stdin.rs", cli / "src/tonal_queue.rs")

spirc = connect / "src/spirc.rs"
text = spirc.read_text(encoding="utf-8")
text = replace(text, "use tokio::{sync::mpsc, time::sleep};",
               'use tokio::{sync::mpsc, time::sleep};\n\n#[path = "queue_edit.rs"]\nmod queue_edit;')
text = replace(text, "    Load(LoadRequest),", """    Load(LoadRequest),
    EditQueue(Vec<String>, tokio::sync::oneshot::Sender<bool>),""")
text = replace(text, "        match cmd {\n            SpircCommand::Shutdown", """        match cmd {
            SpircCommand::EditQueue(tracks, ack) => {
                let accepted = self.edit_queue(tracks);
                let _ = ack.send(matches!(accepted, Ok(true)));
                return accepted.map(|_| ());
            }
            SpircCommand::Shutdown""")
spirc.write_text(text, encoding="utf-8")

# Same start-of-track marker as android/native/vendor/librespot-playback: one
# 8-byte f32 frame written to the pipe before the first sample of each track.
# AudioEngine strips it and uses it as the exact end of the previous track.
player = playback / "src/player.rs"
text = player.read_text(encoding="utf-8")
text = replace(text, """        if start_playback {
            self.ensure_sink_running();
            self.send_event(PlayerEvent::Playing {""", """        if start_playback {
            self.ensure_sink_running();
            // Tonal: exact start of this track inside the PCM stream (transitions).
            let _ = self.sink.write(
                AudioPacket::Raw(tonal_track_marker(position_ms)),
                &mut self.converter,
            );
            self.send_event(PlayerEvent::Playing {""")
text = replace(text, "impl PlayerInternal {\n    fn ensure_sink_running(&mut self) {", """/// Tonal: signalling NaN on the left, quiet NaN carrying the start position
/// (ms, 22 bits) on the right. Never audio; the reader removes the frame.
pub const TONAL_TRACK_MARKER: u32 = 0x7FA0_7A1B;
fn tonal_track_marker(position_ms: u32) -> Vec<u8> {
    let mut bytes = Vec::with_capacity(8);
    bytes.extend_from_slice(&TONAL_TRACK_MARKER.to_le_bytes());
    bytes.extend_from_slice(&(0x7FC0_0000u32 | position_ms.min(0x3F_FFFF)).to_le_bytes());
    bytes
}

impl PlayerInternal {
    fn ensure_sink_running(&mut self) {""")
player.write_text(text, encoding="utf-8")

main = cli / "src/main.rs"
text = main.read_text(encoding="utf-8")
anchor = "    loop {\n        tokio::select! {\n            credentials = async {"
text = replace(text, anchor, """    let mut queue_edits = tonal_queue::start();
    loop {
        tokio::select! {
            command = async {
                match queue_edits.as_mut() {
                    Some(rx) => rx.recv().await,
                    None => std::future::pending().await,
                }
            } => {
                if let Some(command) = command {
                    tonal_queue::handle(&command, spirc.as_ref());
                } else {
                    queue_edits = None;
                }
            },
            credentials = async {""")
main.write_text(text + "\nmod tonal_queue;\n", encoding="utf-8")

manifest = cli / "Cargo.toml"
with manifest.open("a", encoding="utf-8") as output:
    output.write('\n[patch.crates-io]\nlibrespot-connect = { path = "../librespot-connect-0.8.0" }\n'
                 'librespot-playback = { path = "../librespot-playback-0.8.0" }\n')
lock = cli / "Cargo.lock"
text = lock.read_text(encoding="utf-8")
for crate in ("librespot-connect", "librespot-playback"):
    text, count = re.subn(
        rf'(name = "{crate}"\nversion = "0\.8\.0"\n)source = "[^"]+"\nchecksum = "[^"]+"\n',
        r"\1", text,
    )
    if count != 1:
        raise RuntimeError(f"Unexpected upstream {crate} lock entry")
lock.write_text(text, encoding="utf-8")
