"""Skips tracks on the real song page and checks the Christmas cover keeps moving.

Dedicated AVD only. Synthetic tone on a real AudioTrack; the page rebuilds as on a real skip.
"""
from pathlib import Path
import io, json, re, sys, time
from PIL import Image, ImageChops
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/playlist'))
import android_ui as ui
OUT = ROOT / 'out/android/christmas-skip'
OUT.mkdir(parents=True, exist_ok=True)
EVERY = 9000


def shot():
    return Image.open(io.BytesIO(ui.adb('exec-out', 'screencap', '-p', binary=True))).convert('RGB')


def cover_moves(name):
    """Santa's corner and the bulbs change between shots only if the cover is animating."""
    a = shot()
    time.sleep(.7)
    b = shot()
    a.save(OUT / f'{name}.png')
    w, h = a.size
    box = (0, int(h * .13), w, int(h * .52))
    diff = ImageChops.difference(a.crop(box), b.crop(box)).convert('L')
    return sum(1 for v in diff.tobytes() if v > 24)


def frames():
    found = re.search(r'Total frames rendered: (\d+)', ui.adb('shell', 'dumpsys', 'gfxinfo', ui.PKG))
    return int(found.group(1)) if found else 0


def main():
    assert ui.adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1' and 'Tonal_Local_20261015' in ui.adb('emu', 'avd', 'name')
    ui.adb('shell', 'am', 'force-stop', ui.PKG)
    ui.adb('logcat', '-c')
    ui.adb('shell', 'am', 'start', '-W', '-n', ui.PKG + '/app.tonal.android.MainActivity', '--ez', 'songPageFixture', 'true',
           '--ez', 'fixtureAudio', 'true', '--ei', 'fixtureSkipMs', str(EVERY), '--ei', 'fixtureStallMs', sys.argv[1] if len(sys.argv) > 1 else '0', '--es', 'themeFixture', 'natal', '--es', 'fixtureBackground', 'Sem animação')
    time.sleep(6)
    rows = [{'phase': 'before skip', 'changed': cover_moves('before')}]
    seen = 0
    end = time.monotonic() + 44
    while time.monotonic() < end:
        logs = ui.adb('logcat', '-d', '-s', 'TonalTest:I')
        done = len(re.findall(r'SKIP_DONE', logs))
        if done > seen:
            seen = done
            time.sleep(3.5)
            ui.adb('shell', 'dumpsys', 'gfxinfo', ui.PKG, 'reset')
            time.sleep(1.5)
            rows.append({'phase': f'after skip {seen}', 'changed': cover_moves(f'skip-{seen}'), 'framesIn2s': frames()})
        time.sleep(.5)
    ui.adb('shell', 'am', 'force-stop', ui.PKG)
    print(json.dumps(rows, ensure_ascii=False))
    print(re.findall(r'STALL underruns=\d+', ui.adb('logcat', '-d', '-s', 'TonalTest:I'))[:6])
    assert seen >= 3, 'not enough skips'
    assert all(r['changed'] > 150 for r in rows), 'the cover stopped after a track change'
    print('CHRISTMAS_SKIP_PASS')


if __name__ == '__main__':
    main()
