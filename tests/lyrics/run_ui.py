"""Exercise the actual LyricsPage with isolated services and synthetic text, without account/network access."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/lyrics-inline-ui'
QT = Path(os.environ.get('QTDIR', str(ROOT / 'out/tools/Qt/6.8.3/msvc2022_64')))


def main():
    module = OUT / 'LyricsHarness'
    module.mkdir(parents=True, exist_ok=True)
    (module / 'qmldir').write_text('module LyricsHarness\nsingleton App 1.0 App.qml\nsingleton Lyrics 1.0 Lyrics.qml\nsingleton Theme 1.0 Theme.qml\n', encoding='utf-8')
    shutil.copyfile(ROOT / 'qml/Theme.qml', module / 'Theme.qml')
    (module / 'Lyrics.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject { enum Status { Idle, Loading, Synced, Plain, Instrumental, NotFound, Error } }\n', encoding='utf-8')
    (module / 'App.qml').write_text('''pragma Singleton
import QtQuick
QtObject {
    function dominantColor(url, callback) { callback("#344455") }
    property QtObject player: QtObject {
        property string trackUri: "spotify:track:fixture"
        property string coverUrl: ""
        property int positionMs: 0
        property bool isPlaying: false
        property string trackName: "Synthetic song"
        property string artistsText: "Example artist"
        property string albumName: "Example album"
        property bool liked: false
        function togglePlay(){isPlaying=!isPlaying}
        function toggleLike(){liked=!liked}
        signal positionChanged()
        signal stateChanged()
        signal trackChanged()
        function seek(ms) { positionMs = ms }
    }
    property QtObject lyrics: QtObject {
        property bool active: false
        property int status: 3
        property var lines: [{time:-1,text:"Hello world"},{time:-1,text:""},{time:-1,text:"A synthetic verse"}]
        signal changed()
        function lineAt(ms) { return 0 }
        function lineTime(i) { return -1 }
        function retry() {}
    }
    property QtObject music: QtObject {
        property var current: ({uri:"spotify:track:fixture"})
        property var lines: []
        property string lyricStatus: ""
        property bool busy: false
        property string lastLanguage: ""
        property int translateCalls: 0
        function useDisplayedLyrics(uri, source) {
            if (uri !== current.uri) return false
            lines = source.slice()
            return true
        }
        function translate(language) { translateCalls++; lastLanguage=language; busy=true; lyricStatus="Traduzindo para pt-BR" }
    }
}
''', encoding='utf-8')
    # Use the production page, changing only its module import for the test doubles.
    (OUT / 'LyricsPage.qml').write_text('import LyricsHarness 1.0\n' + (ROOT / 'qml/pages/LyricsPage.qml').read_text(encoding='utf-8'), encoding='utf-8')
    shutil.copyfile(ROOT / 'qml/components/ThinScrollBar.qml', OUT / 'ThinScrollBar.qml')
    (OUT / 'Chip.qml').write_text('import QtQuick.Controls\nButton {}\n', encoding='utf-8')
    shutil.copyfile(Path(__file__).with_name('tst_inline.qml'), OUT / 'tst_inline.qml')
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QT_QUICK_CONTROLS_STYLE='Basic',
               QT_PLUGIN_PATH=str(QT / 'plugins'), QML_IMPORT_PATH=str(QT / 'qml'),
               QT_QPA_FONTDIR=str(Path(env['WINDIR']) / 'Fonts'))
    result = subprocess.run([str(QT / 'bin/qmltestrunner.exe'), '-input', str(OUT / 'tst_inline.qml'),
                             '-import', str(OUT), '-o', str(OUT / 'results.txt') + ',txt'],
                            env=env, cwd=OUT, timeout=35, capture_output=True, text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    (OUT / 'stderr.log').write_text(result.stderr, encoding='utf-8')
    print((OUT / 'results.txt').read_text(encoding='utf-8'))
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
