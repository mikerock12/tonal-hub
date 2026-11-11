"""Prepare the pinned upstream CLI with Tonal's shared queue-only command."""
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
    output.write('\n[patch.crates-io]\nlibrespot-connect = { path = "../librespot-connect-0.8.0" }\n')
lock = cli / "Cargo.lock"
text = lock.read_text(encoding="utf-8")
text, count = re.subn(
    r'(name = "librespot-connect"\nversion = "0\.8\.0"\n)source = "[^"]+"\nchecksum = "[^"]+"\n',
    r"\1", text,
)
if count != 1:
    raise RuntimeError("Unexpected upstream librespot-connect lock entry")
lock.write_text(text, encoding="utf-8")
