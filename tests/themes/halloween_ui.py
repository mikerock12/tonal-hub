"""Render the actual themed components with original synthetic art and lyrics.

Run tests/mix/run_ui.py first to prepare the existing account-free QML module.
Captures demonstrate UI only, never Spotify playback or physical device output.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/halloween-ui'
OUT.mkdir(parents=True, exist_ok=True)
from PIL import Image, ImageDraw
art = Image.new('RGB', (400, 400), '#34475b')
draw = ImageDraw.Draw(art)
draw.ellipse((84, 64, 316, 296), fill='#b87042')
draw.ellipse((158, 138, 242, 222), fill='#34475b')
draw.text((110, 340), 'SYNTHETIC ART', fill='#fff4e9')
art.save(OUT / 'cover.png')
(OUT / 'cover.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400"><rect width="400" height="400" fill="#34475b"/><circle cx="200" cy="180" r="116" fill="#b87042"/><circle cx="200" cy="180" r="42" fill="#34475b"/><text x="200" y="345" text-anchor="middle" fill="#fff4e9" font-family="sans-serif" font-size="28">CAPA SINTÉTICA</text></svg>''', encoding='utf-8')
(OUT / 'tst_halloween.qml').write_text('''import QtQuick
import QtQuick.Controls
import QtTest
import MixHarness 1.0
import "../mix-ui"
Pane {
 id:root;width:1000;height:780;padding:0;font.family:"Segoe UI"
 background:Rectangle{color:Theme.windowBg}
 Cover{id:cover;x:30;y:70;width:310;height:310;source:Qt.resolvedUrl("cover.png")}
 Label{x:30;y:22;text:"HALLOWEEN · DADOS SINTÉTICOS";color:Theme.textPrimary;font.pixelSize:22}
 Label{x:30;y:410;text:"Música de demonstração";color:Theme.textPrimary;font.pixelSize:24}
 Label{x:30;y:448;text:"Artista sintético · Álbum de teste";color:Theme.textSecondary}
 SeekBar{id:seek;x:30;y:490;width:310;value:.61;showThumb:true}
 PlayButton{id:play;x:155;y:550;diameter:64;tooltip:""}
 ActionIcon{id:delayedIcon;x:250;y:550;width:0;height:0;visible:false;symbol:"play";ink:"#ff954e";background:Rectangle{color:"#1b101d"}}
 Rectangle{x:375;y:70;width:595;height:660;radius:20;color:Theme.panel
  LyricBackdrop{id:stage;anchors.fill:parent;active:true;playing:false;mode:"Halloween"}
  Label{id:modeLabel;x:25;y:18;text:stage.mode; color:Theme.textPrimary;font.pixelSize:22}
  Column{x:25;y:115;spacing:24
   Repeater{model:["A noite abre outro caminho","Amanhã traz novas cores","Versos originais para este teste"]
    Column{required property string modelData;spacing:10
     Label{text:modelData;color:"#fff4e9";font.pixelSize:26;font.bold:true}
     Label{text:"Tradução sintética para demonstração";color:"#d6bfc5";font.pixelSize:18}
    }
   }
  }
 }
 TestCase {
  name:"HalloweenVisual";when:windowShown
  function test_switch_cover_controls_and_modes(){
   Theme.setTheme("tonal");tryCompare(cover,"ready",true,15000);wait(150)
   grabImage(cover).save("cover-tonal.png")
   Theme.setTheme("halloween");compare(Theme.seek.thumbShape,"drop");wait(180)
   grabImage(cover).save("cover-halloween.png")
   delayedIcon.width=64;delayedIcon.height=64;delayedIcon.visible=true;wait(120);grabImage(delayedIcon).save("late-icon.png");delayedIcon.visible=false
   mouseClick(play);mouseMove(root,5,5);wait(420);verify(seek.thumbVisible)
   const flow=findChild(seek,"bloodSurface");verify(flow!==null);compare(flow.animationRunning,false)
   flow.phase=.15;wait(80);grabImage(seek).save("blood-frame-a.png")
   flow.phase=1.4;wait(80);grabImage(seek).save("blood-frame-b.png")
   let still=flow.phase;wait(160);compare(flow.phase,still)
   seek.playing=true;wait(170);if(flow.animationRunning)verify(flow.phase>still)
   seek.playing=false;still=flow.phase;wait(170);compare(flow.phase,still)
   for(let m of ["Halloween","Bruxas","Monstros","Vampiros"]){stage.mode=m;stage.phase=1;wait(120);grabImage(stage).save("backdrop-"+m+".png");grabImage(stage.parent.parent).save("player-"+m+".png")}
   let phase=stage.phase;wait(160);compare(stage.phase,phase)
   stage.active=false;stage.playing=true;wait(160);compare(stage.phase,phase)
   stage.playing=false;stage.active=true
   cover.round=true;wait(160);grabImage(cover).save("artist-halloween.png")
   cover.round=false;Theme.setTheme("tonal");wait(160);grabImage(cover).save("cover-restored.png")
   Theme.setTheme("halloween");wait(160);grabImage(cover).save("cover-halloween-again.png")
  }
 }
}''', encoding='utf-8')
qt = Path(os.environ.get('QTDIR', ROOT / 'out/tools/Qt/6.8.3/msvc2022_64'))
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QT_QUICK_CONTROLS_STYLE='Basic',
           QT_PLUGIN_PATH=str(qt / 'plugins'), QML_IMPORT_PATH=str(ROOT / 'out/mix-ui'),
           QT_QPA_FONTDIR=str(Path(os.environ['WINDIR']) / 'Fonts'))
result = subprocess.run([str(qt / 'bin/qmltestrunner.exe'), '-input', str(OUT / 'tst_halloween.qml'),
                         '-o', str(OUT / 'results.txt') + ',txt'], cwd=OUT, env=env,
                        capture_output=True, text=True, timeout=45,
                        creationflags=subprocess.CREATE_NO_WINDOW)
print((OUT / 'results.txt').read_text(encoding='utf-8'))
if result.returncode:
    print(result.stderr[-3000:])
    raise SystemExit(result.returncode)
from PIL import Image, ImageChops
original = Image.open(OUT / 'cover-tonal.png').convert('RGB')
themed = Image.open(OUT / 'cover-halloween.png').convert('RGB')
assert ImageChops.difference(original, themed).getbbox(), 'Live theme switch did not decorate the loaded cover'
assert not ImageChops.difference(original, Image.open(OUT / 'cover-restored.png').convert('RGB')).getbbox(), 'Decoration remained after restoring the theme'
assert not ImageChops.difference(themed, Image.open(OUT / 'cover-halloween-again.png').convert('RGB')).getbbox(), 'Decoration failed after switching back'
images = [Image.open(OUT / ('backdrop-' + mode + '.png')).tobytes() for mode in ('Halloween', 'Bruxas', 'Monstros', 'Vampiros')]
assert len(set(images)) == 4, 'The four scenes must render differently'
icon = Image.open(OUT / 'late-icon.png').convert('RGB').crop((22, 22, 44, 44))
assert sum(1 for r,g,b in icon.getdata() if r > 200 and 70 < g < 200 and b < 140) > 30, 'The icon stayed blank after delayed layout'
assert Image.open(OUT / 'blood-frame-a.png').tobytes() != Image.open(OUT / 'blood-frame-b.png').tobytes(), 'The blood streams did not evolve between phases'
print('HALLOWEEN_VISUAL_PASS: loaded art, live switch/restore, controls, four distinct scenes; paused/hidden stages remain still')
