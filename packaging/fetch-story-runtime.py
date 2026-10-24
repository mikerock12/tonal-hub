"""Fetch the pinned native runtime, never the optional model weights."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SPEC = json.loads((ROOT / "resources/local-stories-model.json").read_text(encoding="utf-8"))
DEST = ROOT / "out/story-deps"
DEST.mkdir(parents=True, exist_ok=True)
archive = DEST / "llama.cpp.tar.gz"

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

if not archive.exists() or sha(archive) != SPEC["runtime_archive_sha256"]:
    partial = archive.with_suffix(".part")
    urllib.request.urlretrieve("https://codeload.github.com/ggml-org/llama.cpp/tar.gz/" + SPEC["runtime_revision"], partial)
    if sha(partial) != SPEC["runtime_archive_sha256"]:
        partial.unlink()
        raise SystemExit("Native runtime checksum mismatch")
    partial.replace(archive)
source = DEST / "llama"
marker = source / ".tonal-extracted"
if not marker.exists() or marker.read_text().strip() != SPEC["runtime_revision"]:
    allowed = {"CMakeLists.txt", "LICENSE", "cmake", "src", "ggml", "include", "vendor", "common", "convert_hf_to_gguf.py"}
    with tarfile.open(archive) as package:
        for entry in package:
            parts = PurePosixPath(entry.name).parts[1:]
            if not parts or parts[0] not in allowed or ".." in parts or not entry.isfile():
                continue
            target = source.joinpath(*parts)
            if not target.resolve().is_relative_to(source.resolve()):
                raise SystemExit("Unsafe runtime archive path")
            target.parent.mkdir(parents=True, exist_ok=True)
            with package.extractfile(entry) as stream:
                target.write_bytes(stream.read())
    marker.write_text(SPEC["runtime_revision"])
print("Verified llama.cpp", SPEC["runtime_revision"], source)
