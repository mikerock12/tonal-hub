import os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'out/text-playlist-ui';OUT.mkdir(parents=True,exist_ok=True)
QT=Path(os.environ.get('QTDIR',ROOT/'out/tools/Qt/6.8.3/msvc2022_64'))
for source in [ROOT/'qml/pages/TextPlaylistDialog.qml',Path(__file__).with_name('tst_playlist.qml')]:shutil.copyfile(source,OUT/source.name)
fonts=OUT/'fonts';fonts.mkdir(exist_ok=True)
for name in ['segoeui.ttf','segoeuib.ttf','segoeuii.ttf','seguisb.ttf']:
    source=Path(os.environ['WINDIR'])/'Fonts'/name
    if source.is_file():shutil.copyfile(source,fonts/name)
env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QT_QUICK_CONTROLS_STYLE='Basic',QT_PLUGIN_PATH=str(QT/'plugins'),QML_IMPORT_PATH=str(QT/'qml'),QT_QPA_FONTDIR=str(fonts))
try:
    r=subprocess.run([str(QT/'bin/qmltestrunner.exe'),'-input',str(OUT/'tst_playlist.qml'),'-o',str(OUT/'results.txt')+',txt'],cwd=OUT,env=env,capture_output=True,text=True,timeout=45,creationflags=subprocess.CREATE_NO_WINDOW)
except subprocess.TimeoutExpired as error:
    detail=error.stderr or b''
    if isinstance(detail,bytes):detail=detail.decode('utf-8',errors='replace')
    (OUT/'stderr.log').write_text(detail[-100000:],encoding='utf-8')
    print(detail[-5000:]);raise
(OUT/'stderr.log').write_text(r.stderr,encoding='utf-8');print((OUT/'results.txt').read_text(encoding='utf-8'))
if r.returncode:print(r.stderr)
raise SystemExit(r.returncode)
