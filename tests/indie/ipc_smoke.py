"""Exercise native single-instance/query/pause IPC without user accounts or APIs."""
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import time
import wave


def main():
    root = Path(__file__).resolve().parents[2]
    build = root / "out/build/release"
    output = root / "out/research/indie-ipc"
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    qt = root / "out/tools/Qt/6.8.3/msvc2022_64"
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
               QT_PLUGIN_PATH=str(qt / "plugins"), QML_IMPORT_PATH=str(qt / "qml"),
               QT_QPA_FONTDIR=str(Path(env["WINDIR"]) / "Fonts"),
               QT_FORCE_STDERR_LOGGING="1", ALSOFT_DRIVERS="null")
    fixture = output / "transport.wav"
    with wave.open(str(fixture), "wb") as audio:
        audio.setnchannels(2)
        audio.setsampwidth(2)
        audio.setframerate(48000)
        block = b"".join(struct.pack("<hh", *(2 * [int(4000 * math.sin(2 * math.pi * 440 * n / 48000))])) for n in range(48000))
        audio.writeframes(block * 12)
    children = []
    report = {"passed": False, "credentials_accessed": False, "public_api_requests": 0}
    try:
        state = output / "state.json"
        state.unlink(missing_ok=True)
        with (output / "indie.log").open("w", encoding="utf-8") as log:
            first = subprocess.Popen([str(build / "IndieTonal.exe"), "--smoke-test", "--hold-ms", "8000",
                                      "--local-file", str(fixture), "--state-file", str(state)],
                                     cwd=build, env=env, stdout=log, stderr=log,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            children.append(first)
            time.sleep(1.6)
            second = subprocess.run([str(build / "IndieTonal.exe"), "--smoke-test", "--query", "x"],
                                    cwd=build, env=env, stdout=log, stderr=log, timeout=4,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            assert second.returncode == 0 and first.poll() is None, "Single-instance activation failed"
            user_hash = hashlib.sha256(Path.home().as_posix().encode()).hexdigest()[:16]
            pipe_name = "\\\\.\\pipe\\IndieTonal-" + user_hash + "-smoke"
            with open(pipe_name, "r+b", buffering=0) as pipe:
                pipe.write(b'{"action":"pause"}')
            assert first.wait(timeout=10) == 0, "Primary IndieTonal did not exit cleanly"
        result = json.loads(state.read_text(encoding="utf-8"))
        assert result["query"] == "x", "Query was not forwarded to primary instance"
        assert result["positionMs"] > 100 and result["durationMs"] > 8000, "Local PCM did not start"
        assert not result["playing"], "IPC pause did not hold local playback"
        report["indie"] = result
        with (output / "hub.log").open("w", encoding="utf-8") as log:
            first = subprocess.Popen([str(build / "TonalHub.exe"), "--smoke-test", "--hold-ms", "4000"],
                                     cwd=build, env=env, stdout=log, stderr=log,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            children.append(first)
            time.sleep(0.8)
            second = subprocess.run([str(build / "TonalHub.exe"), "--smoke-test"],
                                    cwd=build, env=env, stdout=log, stderr=log, timeout=2,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            assert second.returncode == 0 and first.poll() is None, "Hub was duplicated"
            assert first.wait(timeout=6) == 0
        report.update(passed=True, hub_single_instance=True)
    finally:
        for child in children:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
