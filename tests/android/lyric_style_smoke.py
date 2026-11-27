"""Lyric style dialog 0.18.2 on the dedicated AVD: font and effect menus drawn with the option itself,
rainbow rail 0..1000 with -/+ fine steps. Synthetic: no Spotify, no physical phone."""
from pathlib import Path
import json, re, sys, time
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/playlist'))
import android_ui as ui
OUT = ROOT / 'out/android/lyric-style-0182'; OUT.mkdir(parents=True, exist_ok=True)


def by_desc(nodes, prefix):
    return [n for n in nodes if n.get('content-desc', '').startswith(prefix)]


def rail_value(nodes):
    for n in nodes:
        m = re.match(r'^(#[0-9A-F]{6}) · (\d+)$', n.get('text', ''))
        if m:
            return m.group(1), int(m.group(2))
    raise AssertionError('rail value not visible')


def shot(name):
    (OUT / name).write_bytes(ui.adb('exec-out', 'screencap', '-p', binary=True))


def main():
    assert ui.adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1' and 'Tonal_Local_20261015' in ui.adb('emu', 'avd', 'name')
    package = ui.PKG; checks = []
    ui.adb('logcat', '-c')
    try:
        ui.adb('shell', 'am', 'force-stop', package)
        ui.adb('shell', 'am', 'start', '-W', '-n', package + '/app.tonal.android.MainActivity', '--ez', 'openLyricStyle', 'true')
        ui.wait_for('Letra original'); time.sleep(.5); shot('dialog.png')
        nodes = ui.dump()
        hex_before, position = rail_value(nodes)
        assert hex_before == '#F5F5F5' or position > 0, (hex_before, position)   # stored color shown until the rail is touched
        plus = by_desc(nodes, 'Cor do texto: mais 1')[0]
        ui.tap(plus); time.sleep(.6); hex_after, after = rail_value(ui.dump())
        assert after == min(1000, position + 1), (position, after)
        minus = by_desc(ui.dump(), 'Cor do texto: menos 1')[0]
        ui.tap(minus); time.sleep(.6); _, back = rail_value(ui.dump())
        assert back == position, (position, back)
        checks.append('rail_plus_minus_fine_step')
        rail = by_desc(ui.dump(), 'Cor do texto: posição de 0 a 1000')[0]
        x0, y0, x1, y1 = map(int, re.findall(r'\d+', rail.get('bounds')))
        ui.adb('shell', 'input', 'tap', str(x1 - 30), str((y0 + y1) // 2)); time.sleep(.6)
        dark_hex, dark = rail_value(ui.dump())
        assert dark > 900, dark
        checks.append('rail_reaches_black_end')
        shot('rail.png')
        font = by_desc(ui.dump(), 'Fonte da letra')[0]
        ui.tap(font); time.sleep(.8); nodes = ui.dump()
        fonts = [n.get('content-desc', '')[6:] for n in by_desc(nodes, 'Fonte ') if not n.get('content-desc', '').startswith('Fonte da ')]
        shot('fonts.png')
        assert len(fonts) >= 8 and any('Aa Bb' in n.get('text', '') for n in nodes), fonts
        all_fonts = list(dict.fromkeys(fonts))
        assert len(all_fonts) >= 10, all_fonts
        checks.append('font_menu_lists_%d_fonts_in_their_typeface' % len(all_fonts))
        current = [n for n in by_desc(nodes, 'Fonte ') if not n.get('content-desc', '').startswith('Fonte da ')][0]
        ui.tap(current); time.sleep(.6)   # close the menu by choosing the current option (BACK would close the dialog too)
        effect = by_desc(ui.dump(), 'Efeito da letra')[0]
        ui.tap(effect); time.sleep(.8)
        for _ in range(12): ui.adb('shell', 'input', 'keyevent', '19')   # the menu opens at the chosen effect: go to the top first
        time.sleep(.6); nodes = ui.dump()
        effects = [n.get('content-desc', '')[7:] for n in by_desc(nodes, 'Efeito ') if not n.get('content-desc', '').startswith('Efeito da ')]
        shot('effects.png')
        # A drag gesture dismisses the Spinner menu on this AVD: scroll it with the D-pad instead.
        for _ in range(12): ui.adb('shell', 'input', 'keyevent', '20')
        time.sleep(.8); nodes = ui.dump()
        effects += [n.get('content-desc', '')[7:] for n in by_desc(nodes, 'Efeito ') if not n.get('content-desc', '').startswith('Efeito da ')]
        shot('effects-end.png')
        all_effects = list(dict.fromkeys(effects))
        assert len(all_effects) >= 10 and 'Vazado' in all_effects and 'Gradiente' in all_effects, all_effects
        checks.append('effect_menu_lists_%d_previewed_effects' % len(all_effects))
        vazado = by_desc(nodes, 'Efeito Vazado')[0]
        ui.tap(vazado); time.sleep(.8); shot('effect-chosen.png')
        assert by_desc(ui.dump(), 'Efeito da letra'), 'dialog closed after choosing an effect'
        checks.append('effect_chosen_from_preview_menu')
        assert 'FATAL EXCEPTION' not in ui.adb('logcat', '-d', '-s', 'AndroidRuntime:E')
        checks.append('no_fatal_exception')
        report = {'passed': True, 'checks': checks, 'fonts': all_fonts, 'effects': all_effects, 'rail': {'start': [hex_before, position], 'plus': [hex_after, after], 'end_tap': [dark_hex, dark]},
                  'synthetic': True, 'physicalPhone': False}
        (OUT / 'result.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8'); print(json.dumps(report, ensure_ascii=False))
    except Exception as error:
        (OUT / 'result.json').write_text(json.dumps({'passed': False, 'checks': checks, 'error': str(error)}, indent=2, ensure_ascii=False), encoding='utf-8'); raise
    finally:
        ui.adb('shell', 'am', 'force-stop', package)


if __name__ == '__main__':
    main()
