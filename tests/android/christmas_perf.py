"""Cost of the Christmas song page on the dedicated AVD, with a synthetic tone on a real AudioTrack.

The same page is measured under the Christmas theme and an ordinary theme. An emulator on a
shared host: the numbers compare the two runs and are not a phone benchmark.
"""
from pathlib import Path
import json, re, sys, time
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/playlist'))
import android_ui as ui
OUT = ROOT / 'out/android/christmas-0159'
OUT.mkdir(parents=True, exist_ok=True)
SECONDS = 60


def pid():
    return ui.adb('shell', 'pidof', ui.PKG).strip().split()[0]


def cpu_ticks(p):
    stat = ui.adb('shell', 'cat', f'/proc/{p}/stat').rsplit(')', 1)[1].split()
    return int(stat[11]) + int(stat[12])


def measure(theme):
    ui.adb('shell', 'am', 'force-stop', ui.PKG)
    ui.adb('logcat', '-c')
    ui.adb('shell', 'am', 'start', '-W', '-n', ui.PKG + '/app.tonal.android.MainActivity', '--ez', 'songPageFixture', 'true',
           '--ez', 'fixtureAudio', 'true', '--es', 'themeFixture', theme, '--es', 'fixtureBackground', 'Sem animação')
    time.sleep(8)
    p = pid()
    ui.adb('shell', 'dumpsys', 'gfxinfo', ui.PKG, 'reset')
    start, begin = cpu_ticks(p), time.monotonic()
    time.sleep(SECONDS)
    used = (cpu_ticks(p) - start) / 100
    elapsed = time.monotonic() - begin
    shot = OUT / f'song-{theme}.png'
    shot.write_bytes(ui.adb('exec-out', 'screencap', '-p', binary=True))
    pss = re.search(r'TOTAL PSS:\s+(\d+)', ui.adb('shell', 'dumpsys', 'meminfo', ui.PKG))
    gfx = ui.adb('shell', 'dumpsys', 'gfxinfo', ui.PKG)
    frames = re.search(r'Total frames rendered: (\d+)', gfx)
    janky = re.search(r'Janky frames: (\d+)', gfx)
    p95 = re.search(r'95th percentile: (\d+)ms', gfx)
    lines = re.findall(r'AUDIO_LOAD frames=(\d+) underruns=(\d+)', ui.adb('logcat', '-d', '-s', 'TonalTest:I'))
    assert lines, 'synthetic tone did not run'
    return {'theme': theme, 'seconds': round(elapsed, 1), 'cpuPercentOfOneCore': round(100 * used / elapsed, 2),
            'pssMiB': round(int(pss.group(1)) / 1024, 1) if pss else None, 'framesRendered': int(frames.group(1)) if frames else None,
            'jankyFrames': int(janky.group(1)) if janky else None, 'p95FrameMs': int(p95.group(1)) if p95 else None,
            'audioFrames': int(lines[-1][0]), 'underruns': int(lines[-1][1])}


def main():
    assert ui.adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1' and 'Tonal_Local_20261015' in ui.adb('emu', 'avd', 'name')
    rows = []
    try:
        for theme in sys.argv[1:] or ['tonal', 'natal', 'tonal', 'natal']:
            rows.append(measure(theme))
            print(json.dumps(rows[-1]), flush=True)
    finally:
        ui.adb('shell', 'am', 'force-stop', ui.PKG)
    (OUT / 'perf.json').write_text(json.dumps({'rows': rows, 'avd': True, 'physicalPhone': False, 'liveSpotify': False}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
