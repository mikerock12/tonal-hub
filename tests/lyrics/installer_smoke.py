"""Install and remove only the new Hub in an isolated workspace folder; never uninstall an existing installation."""
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import time
import winreg

ROOT = Path(__file__).resolve().parents[2]
VERSION=re.search(r'project\(Tonal VERSION ([0-9.]+)',(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
OUT = ROOT / ('out/installer-smoke-'+VERSION.replace('.','')+'0'+os.environ.get('TONAL_INSTALLER_TEST_SUFFIX',''))
TARGET = OUT / 'app'
SETUP = ROOT / f'out/release-local/Tonal-Hub-{VERSION}.0-Windows-x64-Setup.exe'
STAGE = ROOT / f'out/release-local/Tonal-Hub-{VERSION}-local'
KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D47D88D5-B58A-49D2-868C-68A55BC3B4E2}_is1'


def registration():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEY) as key:
            return {name: winreg.QueryValueEx(key, name)[0] for name in ('DisplayVersion', 'InstallLocation')}
    except FileNotFoundError:
        return None


def run(args, **kwargs):
    return subprocess.run([str(x) for x in args], creationflags=subprocess.CREATE_NO_WINDOW,
                          check=True, timeout=120, **kwargs)


def main():
    assert registration() is None, 'A Hub installation already exists; refusing to modify it.'
    assert not TARGET.exists(), 'Test target already exists; refusing to overwrite it.'
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"installer_sha256":hashlib.sha256(SETUP.read_bytes()).hexdigest()}
    run([SETUP, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', '/TASKS=',
         f'/DIR={TARGET}', f'/LOG={OUT / "install.log"}'])
    registered = registration()
    assert registered and Path(registered['InstallLocation']).resolve() == TARGET.resolve()
    report['installation'] = registered
    report['files_match'] = {}
    for name in ('TonalHub.exe', 'Tonalify.exe', 'IndieTonal.exe', 'qt.conf',
                 'platforms/qwindows.dll', 'sqldrivers/qsqlite.dll', 'CREDITS.md', 'WINDOWS_INSTALLER_LOCAL.md'):
        installed = hashlib.sha256((TARGET / name).read_bytes()).hexdigest()
        assert installed == hashlib.sha256((STAGE / name).read_bytes()).hexdigest(), name
        report['files_match'][name] = True
    env = os.environ.copy()
    for key in ('QTDIR', 'QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH'):
        env.pop(key, None)
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', ALSOFT_DRIVERS='null',
               QT_FORCE_STDERR_LOGGING='1', QT_QPA_FONTDIR=str(Path(env['WINDIR']) / 'Fonts'))
    with (OUT / 'startup.log').open('w', encoding='utf-8') as log:
        run([TARGET / 'Tonalify.exe', '--audio-smoke-test', '--local-only', '--audio-settings',
             '--local-file', ROOT / 'out/audio-fixtures/stereo-ui-48000.wav'], cwd=TARGET,
            env=env, stdout=log, stderr=log)
    report['installed_app_startup_without_sdk_plugin_paths'] = True
    # Reinstall to the same location and check that user-added files survive the update.
    sentinel = TARGET / 'test-preserve.txt'
    sentinel.write_text('installer upgrade fixture', encoding='utf-8')
    run([SETUP, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', '/TASKS=',
         f'/DIR={TARGET}', f'/LOG={OUT / "reinstall.log"}'])
    assert sentinel.read_text(encoding='utf-8') == 'installer upgrade fixture'
    report['reinstallation_preserves_unmanaged_files'] = True
    # Remove the test-only sentinel explicitly; Inno must not remove arbitrary user files.
    sentinel.unlink()
    assert Path(registration()['InstallLocation']).resolve() == TARGET.resolve()
    run([TARGET / 'unins000.exe', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
         f'/LOG={OUT / "uninstall.log"}'])
    deadline = time.monotonic() + 20
    while (TARGET.exists() or registration() is not None) and time.monotonic() < deadline:
        time.sleep(0.2)
    assert not TARGET.exists() and registration() is None, 'Isolated uninstall incomplete'
    report['isolated_uninstallation'] = True
    report['passed'] = True
    (OUT / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
