"""Render local native QML screens sequentially in isolated smoke instances."""
import json
import os
from pathlib import Path
import subprocess
import argparse


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--screen", choices=["hub-ui","indie-effects-ui","indie-connections-ui","indie-results-ui","indie-veruca-ui","indie-live-ui"])
    selected=parser.parse_args().screen
    root = Path(__file__).resolve().parents[2]
    qt = root / "out/tools/Qt/6.8.3/msvc2022_64"
    build = root / "out/build/release"
    output = root / "out/research"
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
               QT_PLUGIN_PATH=str(qt / "plugins"), QML_IMPORT_PATH=str(qt / "qml"),
               QT_QPA_FONTDIR=str(Path(env["WINDIR"]) / "Fonts"),
               QT_FORCE_STDERR_LOGGING="1", ALSOFT_DRIVERS="null")
    cases = [("TonalHub", "hub-ui", []),
             ("IndieTonal", "indie-effects-ui", ["--panel", "effects"]),
             ("IndieTonal", "indie-connections-ui", ["--panel", "connections"]),
             ("IndieTonal", "indie-results-ui", ["--query", "Nirvana"]),
             ("IndieTonal", "indie-veruca-ui", ["--query", "Veruca Salt", "--hold-ms", "20000"]),
             ("IndieTonal", "indie-live-ui", ["--query", "King Gizzard & The Lizard Wizard", "--panel", "shows", "--hold-ms", "20000"])]
    results = []
    for program, name, extra in cases:
        if selected and name != selected:
            continue
        picture = output / (name + ".png")
        picture.unlink(missing_ok=True)
        with (output / (name + ".log")).open("w", encoding="utf-8") as log:
            subprocess.run([str(build / (program + ".exe")), "--smoke-test", *extra,
                            "--state-file", str(output / (name + "-state.json")),
                            "--screenshot", str(picture)], cwd=build, env=env,
                           stdout=log, stderr=log, check=True, timeout=25,
                           creationflags=subprocess.CREATE_NO_WINDOW)
        assert picture.is_file() and picture.stat().st_size > 1000, name
        entry={"screen": name, "rendered": True, "bytes": picture.stat().st_size}
        state_path=output / (name + "-state.json")
        if program == "IndieTonal" and state_path.is_file():
            state=json.loads(state_path.read_text(encoding="utf-8"))
            items=state.get("items",[])
            entry.update(first_result=items[0] if items else None,
                         live_shows_loaded=sum(item.get("kind")=="show" for item in items),
                         live_status=state.get("liveStatus"),catalog_busy=state.get("busy"),
                         catalog_messages=state.get("messages"))
        results.append(entry)
    report = {"passed": True, "screens": results, "spotify_credentials_accessed": False}
    report_name="hub-render-tests.json" if not selected else "hub-render-"+selected+"-tests.json"
    (output / report_name).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
