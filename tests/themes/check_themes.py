"""Checks resources/themes.json: complete tokens, readable contrast and the Android copy."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'resources/themes.json'
ANDROID = ROOT / 'android/app/src/main/assets/themes.json'
COLORS = ['bg', 'panel', 'card', 'cardHover', 'text', 'textSecondary', 'textMuted',
          'accent', 'accentHover', 'accentText', 'accentDeep', 'onAccent', 'divider']
SEEK_COLORS = ['track', 'thumb', 'thumbBorder']
SHAPES = {'circle', 'ring', 'pill', 'diamond', 'drop', 'star'}
HEX = re.compile(r'^#[0-9a-fA-F]{6}$')


def luminance(value):
    channels = [int(value[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    linear = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(a, b):
    high, low = sorted((luminance(a), luminance(b)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def main():
    failures = []
    checks = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            failures.append(message)

    raw = SOURCE.read_bytes()
    check(ANDROID.exists() and ANDROID.read_bytes() == raw, 'Android assets/themes.json differs from resources/themes.json')
    data = json.loads(raw.decode('utf-8'))
    themes = data['themes']
    ids = [t.get('id') for t in themes]
    check(len(themes) >= 5, 'at least five themes are required')
    check(len(set(ids)) == len(ids), 'theme ids must be unique')
    check(data.get('default') in ids, 'default theme must exist')
    for theme in themes:
        name = theme.get('id', '?')
        check(bool(theme.get('name')) and bool(theme.get('description')), f'{name}: name and description')
        for key in COLORS:
            check(bool(HEX.match(theme.get(key, ''))), f'{name}: {key} must be #rrggbb')
        seek = theme.get('seek', {})
        for key in SEEK_COLORS:
            check(bool(HEX.match(seek.get(key, ''))), f'{name}: seek.{key} must be #rrggbb')
        for key in ('fill', 'activeFill'):
            stops = seek.get(key, [])
            check(len(stops) == 2 and all(HEX.match(s) for s in stops), f'{name}: seek.{key} needs two colors')
        check(seek.get('thumbShape') in SHAPES, f'{name}: unknown thumb shape')
        check(2 <= seek.get('height', 0) <= seek.get('activeHeight', 0) <= 10, f'{name}: bar heights')
        check(10 <= seek.get('thumbSize', 0) <= 22, f'{name}: thumb size')
        if all(HEX.match(theme.get(k, '')) for k in COLORS):
            # WCAG AA for body text on the panel and on cards; accent text and the
            # label drawn on accent buttons; secondary text at large-text level.
            check(contrast(theme['text'], theme['panel']) >= 7, f'{name}: text/panel contrast')
            check(contrast(theme['text'], theme['card']) >= 4.5, f'{name}: text/card contrast')
            check(contrast(theme['textSecondary'], theme['panel']) >= 4.5, f'{name}: secondary text contrast')
            check(contrast(theme['accentText'], theme['panel']) >= 4.5, f'{name}: accent text contrast')
            check(contrast(theme['onAccent'], theme['accent']) >= 4.5, f'{name}: label on accent contrast')
            check(contrast(seek.get('fill', ['#000000'])[0], seek.get('track', '#000000')) >= 1.5, f'{name}: fill visible on track')
    # Launcher icon per theme (packaging/generate-theme-icons.py) and its alias.
    import importlib.util
    spec = importlib.util.spec_from_file_location('theme_icons', ROOT / 'packaging/generate-theme-icons.py')
    icons = importlib.util.module_from_spec(spec); spec.loader.exec_module(icons)
    manifest = (ROOT / 'android/app/src/main/AndroidManifest.xml').read_text(encoding='utf-8')
    for name, text in icons.expected().items():
        drawable = ROOT / 'android/app/src/main/res/drawable' / name
        check(drawable.exists() and drawable.read_bytes().decode('utf-8').replace('\r\n', '\n') == text, f'{name} out of date: run packaging/generate-theme-icons.py')
    for theme in themes:
        alias = 'HubIcon' + theme['id'][0].upper() + theme['id'][1:]
        check(f'android:name=".{alias}"' in manifest, f'{alias} launcher alias missing')
    # Christmas images are shared, and Santa belongs to the song page alone: album grids
    # stay on one still image, so a page of covers never runs many animations at once.
    seasonal = ROOT / 'resources/seasonal'
    copies = ROOT / 'android/app/src/main/assets/seasonal'
    names = sorted(p.name for p in seasonal.iterdir())
    check(names == sorted(p.name for p in copies.iterdir()), 'Android seasonal assets differ from resources/seasonal')
    for name in names:
        check((seasonal / name).read_bytes() == (copies / name).read_bytes(), f'{name}: Android copy differs')
    garland = json.loads((seasonal / 'garland.json').read_text(encoding='utf-8'))
    check(len(garland['bulbs']) >= 50, 'Garland needs many bulbs')
    check(json.dumps(garland['bulbs'], separators=(',', ':')) in (ROOT / 'qml/companion/ChristmasGarlandData.js').read_text(encoding='utf-8'), 'QML garland data out of date: run packaging/generate-christmas-assets.py')
    santa_users = [p.name for p in (ROOT / 'qml').rglob('*.qml') if p.name != 'ChristmasSanta.qml' and 'ChristmasSanta' in p.read_text(encoding='utf-8')]
    check(santa_users == ['MusicPanel.qml'], f'Santa outside the song page: {santa_users}')
    stage_users = [p.name for p in (ROOT / 'android/app/src/main/java').rglob('*.java') if 'new ChristmasStage(' in p.read_text(encoding='utf-8')]
    check(stage_users == ['MusicPanel.java'], f'Christmas stage outside the song page: {stage_users}')
    panel = (ROOT / 'android/app/src/main/java/app/tonal/android/MusicPanel.java').read_text(encoding='utf-8')
    hero = panel[panel.index('private void songHero('):]
    check('new ChristmasStage(' in hero[:hero.index('\n    private ', 10)], 'Christmas stage must stay in songHero')
    cover_effects = (ROOT / 'qml/companion/CoverEffects.qml').read_text(encoding='utf-8')
    check('AnimatedImage' not in cover_effects and 'ChristmasGarland' not in cover_effects, 'Ordinary covers must stay still')
    if failures:
        print('\n'.join(failures))
        print(f'THEMES_FAIL {len(failures)} of {checks}')
        return 1
    print(f'THEMES_PASS {len(themes)} themes, {checks} checks')
    return 0


if __name__ == '__main__':
    sys.exit(main())
