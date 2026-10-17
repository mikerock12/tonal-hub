"""Verify the portable Windows package using only its own Qt/DSP runtime."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
import zipfile


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def verify_archive(stage):
    with zipfile.ZipFile(str(stage)+".zip") as archive:
        assert archive.testzip() is None,"ZIP CRC verification failed"
        members={info.filename.replace("\\","/") for info in archive.infolist() if not info.is_dir()}
        expected={path.relative_to(stage).as_posix() for path in stage.rglob("*") if path.is_file()}
        assert members==expected,"ZIP contents differ from portable directory"
        for name in ("TonalHub.exe","Tonalify.exe","IndieTonal.exe","qt.conf","TONAL_HUB_LOCAL.md"):
            assert hashlib.sha256(archive.read(name)).hexdigest()==digest(stage/name),name
        return {"passed":True,"files":len(members),"all_crc_verified":True,"executables_match_portable_directory":True}


def main():
    root=Path(__file__).resolve().parents[1]
    stage=root / "out/release-local/Tonal-Hub-0.1.2-local"
    result=root / "out/research"
    env=os.environ.copy()
    for key in ("QT_PLUGIN_PATH","QML_IMPORT_PATH","QML2_IMPORT_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QTDIR"):
        env.pop(key,None)
    windows=Path(env["WINDIR"])
    env.update(PATH=os.pathsep.join((str(stage),str(windows / "System32"),str(windows))),
               QT_QPA_PLATFORM="offscreen",QT_QUICK_BACKEND="software",
               QT_QPA_FONTDIR=str(windows / "Fonts"),ALSOFT_DRIVERS="null",
               QT_FORCE_STDERR_LOGGING="1")
    checks=[]
    for name,args in [
        ("TonalHub.exe",["--smoke-test"]),
        ("IndieTonal.exe",["--smoke-test"]),
        ("Tonalify.exe",["--audio-smoke-test","--local-only","--audio-settings","--local-file",str(root / "out/audio-fixtures/stereo-48000.wav")])]:
        with (result / ("portable-"+name+".log")).open("w",encoding="utf-8") as log:
            run=subprocess.run([str(stage / name),*args],cwd=stage,env=env,stdout=log,stderr=log,
                               timeout=25,creationflags=subprocess.CREATE_NO_WINDOW)
        checks.append({"program":name,"exit_code":run.returncode,"passed":run.returncode==0})
    names={p.name.lower() for p in stage.rglob("*") if p.is_file()}
    forbidden={"credentials.json","test-auth.json","audio.json","tokens.json","oauth.json","settings.ini","playback.log","playback.log.1"}
    assert not names.intersection(forbidden),"Private runtime data found in package"
    test_xml=ET.parse(result / "hub-ctest.xml").getroot()
    tests=[{"name":item.attrib.get("name"),"passed":item.find("failure") is None and item.find("error") is None}
           for item in test_xml.iter("testcase")]
    render=json.loads((result / "hub-render-tests.json").read_text(encoding="utf-8"))
    screens={item["screen"]:item for item in render["screens"]}
    assert screens["indie-veruca-ui"]["first_result"]["id"]=="86e8e21a-d29f-47d2-bbf0-e079f7044e71","Veruca Salt ranking regression"
    assert screens["indie-results-ui"]["first_result"]["id"]=="5b11f4ce-a62d-471e-81fc-a69a8278c7da","Nirvana homonym ranking regression"
    assert screens["indie-live-ui"]["live_shows_loaded"]>0,"Live show discovery returned no items"
    artifacts=[]
    for file in [stage / name for name in ("TonalHub.exe","Tonalify.exe","IndieTonal.exe")]+[Path(str(stage)+".zip")]:
        artifacts.append({"path":file.relative_to(root).as_posix(),"bytes":file.stat().st_size,"sha256":digest(file)})
    report={"version":"Tonal Hub 0.1.2-local","recorded_at_utc":datetime.now(timezone.utc).isoformat(),
            "platform":"Windows x64","local_only":True,"portable_checks":checks,"ctest":tests,
            "portable_tests_passed":all(c["passed"] for c in checks),
            "private_runtime_files_packaged":False,"artifacts":artifacts,"archive_integrity":verify_archive(stage),
            "ipc":json.loads((result / "indie-ipc/result.json").read_text(encoding="utf-8")),
            "render":render,
            "spotify_recovery":json.loads((result / "spotify-recovery-tests.json").read_text(encoding="utf-8")),
            "spotify_live_before_fix":json.loads((result / "spotify-recovery-live-before.json").read_text(encoding="utf-8")),
            "spotify_live_session":json.loads((result / "spotify-recovery-live.json").read_text(encoding="utf-8")),
            "spotify_live_readiness":json.loads((result / "spotify-readiness-live.json").read_text(encoding="utf-8")),
            "live_bands":json.loads((result / "live-bands-probe.json").read_text(encoding="utf-8")),
            "veruca_discography":json.loads((result / "indie-veruca-tests.json").read_text(encoding="utf-8")),
            "live_rock_pcm":json.loads((result / "indie-live-rock-tests.json").read_text(encoding="utf-8")),
            "lma_individual":json.loads((result / "indie-lma-tests.json").read_text(encoding="utf-8")),
            "joint_network_run":json.loads((result / "indie-live-tests-complete.json").read_text(encoding="utf-8")),
            "limits":["Jamendo and authenticated Subsonic/Bandcamp not tested",
                      "Cross-provider identity merges and Spotify exclusivity audit not implemented",
                      "Individual PCM snippets approximately 2 seconds; not a long playback endurance test",
                      "A joint network run failed on a remote 5xx response",
                      "Original user incident had no retained crash/engine logs; exact cause was not reconstructed",
                      "Full live Spotify crash/resume test skipped because playback was active; live readiness used zero playback commands",
                      "Indie catalog network/render evidence retained from 0.1.1; current package GUI/audio smoke tests repeated",
                      "No physical Bluetooth, Android hub, audible or CPU/RAM comparison test"]}
    output=root / "docs/tonal-hub-tests.json"
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"portable_tests_passed":report["portable_tests_passed"],"ctest_passed":all(t["passed"] for t in tests),"report":str(output)},indent=2))
    assert report["portable_tests_passed"] and len(tests)==6 and all(t["passed"] for t in tests)
    assert report["spotify_recovery"]["passed"] and report["spotify_live_readiness"]["passed"]
    assert report["spotify_live_readiness"]["playback_commands_sent"]==0


if __name__=="__main__":
    main()
