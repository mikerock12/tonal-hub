"""Text to Tonalify 0.18.3 drafts and "Adicionar mais músicas" on the dedicated AVD (synthetic fixture, no Spotify writes).
DRAFT-01/02/03/04 and APPEND-01 on Android: exit question with three actions, save, process death, resume, delete,
storage failure; a second list added to the same playlist."""
from pathlib import Path
import json, re, sys, time
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/playlist'))
import android_ui as ui
OUT = ROOT / 'out/android/text-draft-0183'; OUT.mkdir(parents=True, exist_ok=True)
PKG = ui.PKG


def texts():
    return [n.get('text', '') for n in ui.dump()]


def find(label, timeout=25):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            nodes = ui.dump()
        except Exception:
            time.sleep(.4); continue
        for n in nodes:
            if n.get('text', '').strip().lower() == label.lower() and n.get('enabled') == 'true':
                return n
        time.sleep(.3)
    raise AssertionError('missing: ' + label)


def click(label):
    ui.tap(find(label)); time.sleep(.5)


def shot(name):
    (OUT / name).write_bytes(ui.adb('exec-out', 'screencap', '-p', binary=True))


def start(*extra):
    ui.adb('shell', 'am', 'force-stop', PKG)
    ui.adb('shell', 'am', 'start', '-W', '-n', PKG + '/app.tonal.android.MainActivity', '--ez', 'openTextPlaylist', 'true', '--ez', 'textPlaylistFixture', 'true',
           '--ez', 'textPlaylistMany', 'true', '--ez', 'textPlaylistMissing', 'true', '--ez', 'textPlaylistUncertain', 'true',
           '--es', 'textAppendText', "'Nova A - Extra 1'", *extra)


def main():
    assert ui.adb('shell', 'getprop', 'ro.kernel.qemu').strip() == '1' and 'Tonal_Local_20261015' in ui.adb('emu', 'avd', 'name')
    checks = []
    ui.adb('logcat', '-c')
    ui.adb('shell', 'run-as', PKG, 'rm', '-rf', 'files/text-drafts')
    try:
        start()
        ui.wait_for('Interpretar e buscar'); ui.click('Interpretar e buscar'); ui.wait_for('Revisão pronta', timeout=60)
        # DRAFT-01: BACK with unsaved work asks with exactly three actions; "Continuar editando" keeps everything.
        ui.adb('shell', 'input', 'keyevent', '4'); find('Sair da criação da playlist?')
        for label in ('Salvar rascunho e sair', 'Sair sem salvar', 'Continuar editando'):
            find(label)
        shot('exit-dialog.png')
        click('Continuar editando'); ui.wait_for('Confira as gravações')
        assert any('30 ativas' in t for t in texts())
        checks.append('exit_question_three_actions_keep_editing')
        # APPEND-01: one more song for the same list; only the new one is searched.
        click('Adicionar mais músicas'); find('Adicionar mais músicas a esta playlist')
        click('Conferir lista'); ui.wait_for('1 referência')
        shot('add-songs.png')
        click('Adicionar à playlist atual'); ui.wait_for('Revisão pronta', timeout=60)
        assert any('31 ativas' in t for t in texts()), texts()
        shot('review-added.png')
        checks.append('append_second_list_same_playlist')
        # DRAFT-02: save, kill the process, reopen and resume with the same counters.
        counters = next(t for t in texts() if ' ativas · ' in t)
        ui.adb('shell', 'input', 'keyevent', '4'); click('Salvar rascunho e sair')
        time.sleep(1)
        assert not any('Confira as gravações' in t for t in texts())
        ui.adb('shell', 'am', 'kill', PKG); ui.adb('shell', 'am', 'force-stop', PKG)
        start(); ui.wait_for('Continuar rascunho')
        assert any(re.search(r'31 músicas · \d+ selecionadas', t) for t in texts()), texts()
        shot('drafts-entry.png')
        click('Continuar'); ui.wait_for('Rascunho aberto')
        assert counters in texts(), (counters, texts())
        checks.append('draft_survives_process_death_and_resumes')
        # Not dirty after resume: BACK closes without the question.
        ui.adb('shell', 'input', 'keyevent', '4'); time.sleep(1)
        assert not any('Sair da criação da playlist?' in t for t in texts())
        # DRAFT-04: leaving without saving keeps the saved draft.
        start(); ui.wait_for('Continuar rascunho'); ui.click('Interpretar e buscar'); ui.wait_for('Revisão pronta', timeout=60)
        ui.adb('shell', 'input', 'keyevent', '4'); click('Sair sem salvar'); time.sleep(.8)
        start(); ui.wait_for('Continuar rascunho')
        assert sum(1 for t in texts() if re.search(r'^\d+ músicas · ', t)) == 1
        checks.append('discard_keeps_previous_draft')
        # Delete with confirmation.
        click('Excluir'); find('Excluir rascunho?'); click('Excluir'); time.sleep(.8)
        assert not any('Continuar rascunho' in t for t in texts())
        checks.append('delete_draft_with_confirmation')
        # DRAFT-03: storage failure is reported and nothing is discarded.
        start('--ez', 'textDraftFailWrite', 'true'); ui.wait_for('Interpretar e buscar'); ui.click('Interpretar e buscar'); ui.wait_for('Revisão pronta', timeout=60)
        ui.adb('shell', 'input', 'keyevent', '4'); click('Salvar rascunho e sair'); ui.wait_for('Não foi possível salvar o rascunho')
        shot('draft-save-failure.png')
        click('Continuar editando'); ui.wait_for('Confira as gravações')
        checks.append('storage_failure_not_claimed_as_saved')
        assert 'FATAL EXCEPTION' not in ui.adb('logcat', '-d', '-s', 'AndroidRuntime:E')
        checks.append('no_fatal_exception')
        report = {'passed': True, 'checks': checks, 'synthetic': True, 'realSpotifyWrites': 0, 'physicalPhone': False}
        (OUT / 'result.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8'); print(json.dumps(report, ensure_ascii=False))
    except Exception as error:
        shot('failure.png')
        (OUT / 'result.json').write_text(json.dumps({'passed': False, 'checks': checks, 'error': str(error)}, indent=2, ensure_ascii=False), encoding='utf-8'); raise
    finally:
        ui.adb('shell', 'am', 'force-stop', PKG)


if __name__ == '__main__':
    main()
