"""Bound teardown of this job's emulator after the instrumented test finishes."""
import os
from pathlib import Path
import signal
import sys
import time


def targets(processes, sdk, port):
    roots = {pid for pid, p in processes.items()
             if p['exe'].is_relative_to(sdk) and p['exe'].name.startswith('qemu-system-')
             and any(p['args'][i:i+2] in (['-port', port], ['-ports', f'{port},{int(port)+1}'])
                     for i in range(len(p['args'])-1))}
    selected = set(roots)
    while True:
        children = {pid for pid, p in processes.items()
                    if p['parent'] in selected and p['exe'].is_relative_to(sdk)}
        if children <= selected:
            break
        selected |= children
    return {pid: processes[pid]['start'] for pid in selected}


def processes():
    found = {}
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            if path.stat().st_uid != os.getuid():
                continue
            fields = (path/'stat').read_text().rsplit(') ', 1)[1].split()
            found[int(path.name)] = dict(parent=int(fields[1]), start=fields[19],
                exe=(path/'exe').resolve(strict=True),
                args=(path/'cmdline').read_bytes().decode(errors='replace').strip('\0').split('\0'))
        except (OSError, IndexError, ValueError):
            pass
    return found


def main():
    if sys.argv[1:] == ['--self-test']:
        sdk = Path('/sdk/emulator')
        fake = {1: dict(parent=0, start='a', exe=sdk/'qemu/qemu-system-x86_64-headless', args=['qemu', '-port', '5554']),
                2: dict(parent=1, start='b', exe=sdk/'crashpad_handler', args=[]),
                3: dict(parent=0, start='c', exe=sdk/'qemu/qemu-system-x86_64-headless', args=['qemu', '-port', '5560']),
                4: dict(parent=3, start='d', exe=sdk/'netsimd', args=[]),
                5: dict(parent=1, start='e', exe=Path('/other/app'), args=[])}
        assert targets(fake, sdk, '5554') == {1: 'a', 2: 'b'}
        assert targets(fake, sdk, '5560') == {3: 'c', 4: 'd'}
        assert targets(fake, sdk, '5570') == {}
        print('CI_EMULATOR_CLEANUP_PASS: only selected SDK emulator and its SDK children')
        return
    if os.environ.get('GITHUB_ACTIONS') != 'true' or sys.platform != 'linux':
        raise SystemExit('Cleanup is restricted to the ephemeral Linux CI runner')
    port = sys.argv[1]
    if not port.isdigit() or not 1024 <= int(port) <= 65534:
        raise SystemExit('Invalid emulator port')
    sdk = (Path(os.environ['ANDROID_HOME'])/'emulator').resolve()
    marker = Path(sys.argv[2])
    if str(marker) != f'/tmp/tonal-emulator-{port}-finished':
        raise SystemExit('Invalid test completion marker')
    selected = targets(processes(), sdk, port)
    print('CI emulator cleanup armed:', port, sorted(selected), flush=True)
    while not marker.exists():
        selected.update(targets(processes(), sdk, port))
        time.sleep(1)
    time.sleep(20)  # let adb emu kill finish normally first
    selected.update(targets(processes(), sdk, port))
    for sig in (signal.SIGTERM, signal.SIGKILL):
        current = processes()
        for pid, start in selected.items():
            p = current.get(pid)
            if p and p['start'] == start and p['exe'].is_relative_to(sdk):
                print('Stopping CI emulator process', pid, p['exe'].name, sig, flush=True)
                try:
                    os.kill(pid, sig)
                except ProcessLookupError:
                    pass
        if sig == signal.SIGTERM:
            time.sleep(5)


if __name__ == '__main__':
    main()
