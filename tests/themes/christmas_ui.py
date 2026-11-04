"""Render the real Christmas components and check how they follow playback.

Run tests/mix/run_ui.py first to prepare the account-free QML module. Art is synthetic
and nothing here exercises Spotify or a physical audio device.
"""
import os
import subprocess
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'out/christmas-ui'
OUT.mkdir(parents=True, exist_ok=True)

art = Image.new('RGB', (600, 600))
draw = ImageDraw.Draw(art)
for y in range(600):
    draw.line((0, y, 600, y), fill=(30 + y // 5, 60 + y // 12, 130 - y // 8))
draw.ellipse((90, 90, 330, 330), fill='#e5bb48')
draw.ellipse((330, 170, 520, 360), fill='#c8483f')
draw.ellipse((150, 320, 430, 590), fill='#4a9f97')
draw.text((30, 20), 'SYNTHETIC ART', fill='#fff4e9')
art.save(OUT / 'cover.png')

(OUT / 'tst_christmas.qml').write_text('''import QtQuick
import QtQuick.Controls
import QtTest
import MixHarness 1.0
import "../mix-ui"
Pane {
 id:root;width:1110;height:900;padding:0
 background:Rectangle{color:"#0c1f18"}
 property bool playing:false
 RightPanel{id:nowPanel;x:760;y:0;width:340;height:640}
 Rectangle{id:card;x:40;y:30;width:680;height:840;radius:24;color:"#11291f"
  Item{id:box;x:40;y:40;width:600;height:600
   ChristmasGarland{id:back;plane:"back";anchors.centerIn:parent;coverWidth:600;coverHeight:600;playing:root.playing}
   Image{id:art;anchors.fill:parent;source:"cover.png"}
   ChristmasGarland{id:front;plane:"front";anchors.centerIn:parent;coverWidth:600;coverHeight:600;playing:root.playing}
   ChristmasSanta{id:santa;width:600*.46;height:width;x:600-width+5;y:600-height+4;musicPlaying:root.playing}
  }
  Cover{id:album;x:0;y:0;width:180;height:180;opacity:.01;source:"cover.png"}
  SeekBar{id:seek;x:40;y:680;width:600;value:.25;showThumb:true;playing:root.playing}
  Label{x:40;y:760;text:"Natal 0.15.9 · dados sintéticos";color:"#efe6c8";font.pixelSize:20}
 }
 TestCase{
  name:"ChristmasVisual";when:windowShown
  function lamps(surface){let found=[];for(let i=0;i<surface.children.length;i++)if(surface.children[i].hasOwnProperty("lit"))found.push(surface.children[i]);return found}
  function test_lights_follow_the_star(){
   Theme.setTheme("natal");wait(150)
   const surface=findChild(seek,"christmasSurface");verify(surface!==null&&surface.visible)
   const all=lamps(surface);verify(all.length>=12)
   const lit=()=>all.filter(l=>l.lit).length
   seek.value=0;wait(30);compare(lit(),0)
   seek.value=.5;wait(30);const half=lit();verify(half>=Math.floor(all.length/2)-1&&half<=Math.ceil(all.length/2)+1)
   seek.value=1;wait(30);compare(lit(),all.length)
   seek.value=.25;wait(450);verify(lit()<all.length/2+1);verify(all[all.length-1].level<.02,"seeking back switches bulbs off")
   wait(800);grabImage(root).save("stage-seek-25.png")
   seek.value=.75;wait(100);grabImage(seek).save("seek-igniting.png");wait(800);grabImage(root).save("stage-seek-75.png")
  }
  function test_santa_runs_only_with_music(){
   tryVerify(()=>santa.status===AnimatedImage.Ready,6000);compare(santa.frameCount,81)
   root.playing=false;wait(120);const held=santa.currentFrame;wait(300);compare(santa.currentFrame,held)
   root.playing=true;tryVerify(()=>santa.currentFrame!==held,2000)
   verify(front.animationRunning&&back.animationRunning)
   const before=front.phase;wait(300);verify(front.phase>before)
   let wrapped=false,last=santa.currentFrame
   for(let i=0;i<90&&!wrapped;i++){wait(50);if(santa.currentFrame<last)wrapped=true;last=santa.currentFrame}
   verify(wrapped,"loop must return to its first frame")
   root.playing=false;wait(120);const paused=santa.currentFrame;const phase=front.phase;wait(300);compare(santa.currentFrame,paused);compare(front.phase,phase);verify(!front.animationRunning)
  }
  function test_now_playing_panel_has_the_scene(){
   Theme.setTheme("natal");Nav.rightPanel="nowplaying";App.player.hasTrack=true;App.player.isPlaying=true;App.player.coverUrlLarge=Qt.resolvedUrl("cover.png");wait(400)
   const santa=findChild(nowPanel,"nowSanta");verify(santa!==null,"Tocando agora needs Santa");tryVerify(()=>santa.visible&&santa.status===AnimatedImage.Ready,5000);compare(santa.frameCount,81)
   const baked=findChild(findChild(nowPanel,"nowGarlandFront"),"garlandImage");tryVerify(()=>baked.status===Image.Ready,5000)
   const first=santa.currentFrame;tryVerify(()=>santa.currentFrame!==first,2000)
   grabImage(nowPanel).save("now-playing.png")
   App.player.isPlaying=false;wait(150);const held=santa.currentFrame;wait(300);compare(santa.currentFrame,held)
   Nav.rightPanel=""
  }
  function test_album_covers_stay_still(){
   Theme.setTheme("natal");wait(100)
   const effects=findChild(album,"coverEffects");verify(effects.visible);verify(!effects.animationRunning,"album covers run no clock")
   const before=effects.phase;wait(250);compare(effects.phase,before)
   verify(findChild(album,"christmasSanta")===null,"Santa belongs to the song page only")
   grabImage(album).save("album-cover.png")
  }
  function test_cover_layers_are_on_both_sides(){
   for(const side of [front,back]){const baked=findChild(side,"garlandImage");tryVerify(()=>baked.status===Image.Ready,5000);verify(baked.sourceSize.width>=1024)}
   front.phase=.4;back.phase=.4;wait(120);grabImage(box).save("layers.png")
   art.visible=false;wait(80);grabImage(box).save("without-cover.png");art.visible=true
   front.visible=false;santa.visible=false;wait(80);grabImage(box).save("back-only.png");front.visible=true;santa.visible=true
  }
 }
}
''', encoding='utf-8')

qt = Path(os.environ.get('QTDIR', ROOT / 'out/tools/Qt/6.8.3/msvc2022_64'))
env = dict(os.environ, PATH=str(qt / 'bin') + os.pathsep + os.environ.get('PATH', ''), QT_QPA_PLATFORM='offscreen',
           QT_PLUGIN_PATH=str(qt / 'plugins'), QML_IMPORT_PATH=str(ROOT / 'out/mix-ui'),
           QT_QPA_FONTDIR=str(Path(os.environ['WINDIR']) / 'Fonts'))
result = subprocess.run([str(qt / 'bin/qmltestrunner.exe'), '-input', str(OUT / 'tst_christmas.qml'),
                         '-o', str(OUT / 'results.txt') + ',txt'], cwd=OUT, env=env, capture_output=True, text=True,
                        timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
print((OUT / 'results.txt').read_text(encoding='utf-8'))
if result.returncode:
    print(result.stderr[-3000:])
    raise SystemExit(result.returncode)

early = Image.open(OUT / 'stage-seek-25.png').convert('RGB')
late = Image.open(OUT / 'stage-seek-75.png').convert('RGB')
assert ImageChops.difference(early, late).getbbox(), 'Moving the star must change the string'
back = Image.open(OUT / 'back-only.png').convert('RGB')
layers = Image.open(OUT / 'layers.png').convert('RGB')
assert ImageChops.difference(back, layers).getbbox(), 'The front garland must cross the cover'
print('CHRISTMAS_VISUAL_PASS: lights follow the star, Santa follows playback, layers on both sides of the cover')
