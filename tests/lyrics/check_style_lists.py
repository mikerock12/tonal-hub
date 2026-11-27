"""Keep the lyric style options identical on Windows (C++/QML) and Android (Java).

Stored names travel in backups between platforms: a font or effect accepted on one side and
rejected on the other would silently fall back to the default after a restore.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def strings(text):
    return re.findall(r'"([^"]*)"', text)


def section(path, pattern):
    match = re.search(pattern, path.read_text(encoding='utf-8'), re.S)
    if not match:
        raise SystemExit(f'FAIL: list not found in {path.relative_to(ROOT)} ({pattern})')
    return match.group(1)


def main():
    cpp = ROOT / 'src/companion/PersonalInsights.cpp'
    js = ROOT / 'qml/companion/LyricColors.js'
    java = ROOT / 'android/app/src/main/java/app/tonal/android/LyricAppearance.java'
    rail_java = ROOT / 'android/app/src/main/java/app/tonal/android/LyricRail.java'
    fonts = {
        'C++': strings(section(cpp, r'lyricFonts\(\)\{return \{(.*?)\};')),
        'QML': strings(section(js, r'var fonts = \[(.*?)\]')),
        'Android': strings(section(java, r'static final String\[\] FONTS=\{(.*?)\};')),
    }
    effects = {
        'C++': strings(section(cpp, r'lyricEffects\(\)\{return \{(.*?)\};')),
        'QML': strings(section(js, r'var effects = \[(.*?)\]')),
        'Android': strings(section(java, r'static final String\[\] EFFECTS=\{(.*?)\};')),
    }
    stops = {
        'QML': section(js, r'var stops = \[(.*?)\]').replace(' ', ''),
        'Android': section(rail_java, r'static final int\[\] STOPS=\{(.*?)\};').replace(' ', ''),
    }
    problems = []
    for name, lists in (('fonts', fonts), ('effects', effects), ('rail stops', stops)):
        values = list(lists.values())
        if any(v != values[0] for v in values):
            problems.append(f'{name} differ: {lists}')
    if len(fonts['C++']) < 10 or len(effects['C++']) < 10:
        problems.append('at least 10 fonts and 10 effects are required')
    if len(stops['QML'].split(',')) != 16:
        problems.append('the rail must have 16 color stops')
    for legacy in ('Sans', 'Serif', 'Mono', 'Rounded'):
        if legacy not in fonts['C++']:
            problems.append(f'legacy font {legacy} must stay accepted for old backups')
    for legacy in ('Normal', 'Contorno', 'Brilho', 'Neon', 'Piscante'):
        if legacy not in effects['C++']:
            problems.append(f'legacy effect {legacy} must stay accepted for old backups')
    if problems:
        raise SystemExit('FAIL: ' + '; '.join(problems))
    print(f'LYRIC_STYLE_LISTS_PASS {len(fonts["C++"])} fonts, {len(effects["C++"])} effects, 16 rail stops on Windows and Android')


if __name__ == '__main__':
    main()
