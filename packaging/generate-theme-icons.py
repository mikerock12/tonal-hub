"""Launcher icons per theme from resources/themes.json (Android vector drawables).

The Tonal Azul icon keeps the original drawable (ic_tonal.xml). Every other
theme gets ic_tonal_<id>.xml: a circle in the theme accent and the sound bars
in the theme's on-accent color (contrast checked by tests/themes/check_themes.py).
Run after editing the theme catalog; --check only verifies the files.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DRAWABLES = ROOT / 'android/app/src/main/res/drawable'
TEMPLATE = '''<vector xmlns:android="http://schemas.android.com/apk/res/android" android:width="48dp" android:height="48dp" android:viewportWidth="48" android:viewportHeight="48">
    <path android:fillColor="{accent}" android:pathData="M24,0A24,24 0,1 0,24 48A24,24 0,1 0,24 0" />
    <path android:strokeColor="{bars}" android:strokeWidth="3" android:strokeLineCap="round" android:pathData="M12,20L12,28M18,14L18,34M24,10L24,38M30,16L30,32M36,21L36,27" />
</vector>
'''


def expected():
    themes = json.loads((ROOT / 'resources/themes.json').read_text(encoding='utf-8'))['themes']
    return {f"ic_tonal_{t['id']}.xml": TEMPLATE.format(accent=t['accent'].upper(), bars=t['onAccent'].upper())
            for t in themes if t['id'] != 'tonal'}


def main():
    files = expected()
    if '--check' in sys.argv:
        stale = [name for name, text in files.items()
                 if not (DRAWABLES / name).exists() or (DRAWABLES / name).read_text(encoding='utf-8') != text]
        print('THEME_ICONS_STALE ' + ' '.join(stale) if stale else f'THEME_ICONS_PASS {len(files)} icons')
        return 1 if stale else 0
    for name, text in files.items():
        (DRAWABLES / name).write_text(text, encoding='utf-8', newline='\n')
    print(f'wrote {len(files)} icons')
    return 0


if __name__ == '__main__':
    sys.exit(main())
