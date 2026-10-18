import os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'out/mix-ui';OUT.mkdir(parents=True,exist_ok=True)
module=OUT/'MixHarness';module.mkdir(exist_ok=True)
(module/'qmldir').write_text('module MixHarness\nsingleton App 1.0 App.qml\nsingleton Theme 1.0 Theme.qml\nsingleton Icons 1.0 Icons.qml\nsingleton Nav 1.0 Nav.qml\nsingleton Playlists 1.0 Playlists.qml\n')
for n in ['Theme','Icons']:shutil.copyfile(ROOT/f'qml/{n}.qml',module/f'{n}.qml')
(module/'Nav.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject{property var me:({id:"user"});function toast(t){}function go(a,b){}function likedSongsUri(){return ""}}')
(module/'Playlists.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject{property var editable:[];function createWithTracks(a){}}')
(module/'App.qml').write_text('''pragma Singleton
import QtQuick
QtObject {
 property string version:"test"
 function dominantColor(url,callback){callback("#344455")}
 property QtObject auth:QtObject{property bool authenticated:true;property string userId:"user"}
 property QtObject music:QtObject{signal changed();function summary(k,v){return {ms:0,plays:0,tracks:[]}}}
 property QtObject player:QtObject{property string contextUri:"";property bool isPlaying:false;property bool shuffle:false;signal playbackRequested();function togglePlay(){}function playContext(a,b,c){}function playUris(a,b){}}
 property QtObject listeningMix:QtObject{property bool busy:false;property string status:"";property var received:({});function cancel(){}function order(n,seed){let a=[];for(let i=1;i<n;i++)a.push(i);a.push(0);return a}function play(rows,index,random,discover,album){received={rows:rows,index:index,random:random,discover:discover,album:album}}}
 property QtObject api:QtObject{
  property int writes:0
  function call(method,path,query,body,callback){if(method!=="GET")writes++;let items=[];for(let i=0;i<6;i++)items.push({uri:"spotify:track:fixture"+i,id:"fixture"+i,name:"Fixture song "+i,duration_ms:180000,artists:[{id:"artist",name:"Test Band"}],album:{id:"album",name:"Test Album",images:[]}});if(path.indexOf("/albums/")===0)callback(200,{id:"album",uri:"spotify:album:album",name:"Test Album",release_date:"1994-01-01",artists:[{id:"artist",name:"Test Band"}],images:[],total_tracks:6,tracks:{items:items,next:null}});else callback(200,{items:[],next:null});}
 }
}
''',encoding='utf-8')
for folder in ['qml/components','qml/companion']:
 for p in (ROOT/folder).glob('*.qml'):
  text=p.read_text(encoding='utf-8');text=text.replace('import QtQuick','import MixHarness 1.0\nimport QtQuick',1);(OUT/p.name).write_text(text,encoding='utf-8')
text=(ROOT/'qml/pages/CollectionPage.qml').read_text(encoding='utf-8').replace('import "qrc:/companion/qml/companion" as Personal','import "." as Personal');(OUT/'CollectionPage.qml').write_text('import MixHarness 1.0\n'+text,encoding='utf-8')
(OUT/'tst_mix.qml').write_text('''import QtQuick
import QtQuick.Controls
import QtTest
import MixHarness 1.0
Pane {width:1000;height:800;padding:0;palette.button:"#263440";palette.buttonText:"#f5f5f5";background:Rectangle{color:"#121212"}
 CollectionPage{id:page;anchors.fill:parent;route:({page:"album",params:{id:"album"}})}
 TestCase{name:"CollectionMix";when:windowShown
  function test_visual_order_and_playback_are_independent(){
   tryCompare(page,"totalTracks",6);let original=page.mixRows();compare(original.length,6);compare(original[0].uri,"spotify:track:fixture0");compare(original[0].artist,"Test Band");compare(original[0].year,1994)
   page.shakeList();verify(page.listShaken);compare(page.mixRows()[0].uri,"spotify:track:fixture1");page.shakeList();verify(!page.listShaken);compare(JSON.stringify(page.mixRows()),JSON.stringify(original));compare(App.api.writes,0)
   page.randomPlayback=true;page.discoveryPlayback=true;page.playFrom(2);compare(App.listeningMix.received.index,2);verify(App.listeningMix.received.random);verify(App.listeningMix.received.discover);compare(App.listeningMix.received.rows.length,6);compare(App.api.writes,0)
   wait(100);grabImage(page).save("collection-modes.png")
  }
 }
}''',encoding='utf-8')
qt=Path(os.environ.get('QTDIR',ROOT/'out/tools/Qt/6.8.3/msvc2022_64'));env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QT_QUICK_CONTROLS_STYLE='Basic',QT_PLUGIN_PATH=str(qt/'plugins'),QML_IMPORT_PATH=str(OUT),QT_QPA_FONTDIR=str(ROOT/'out/text-playlist-ui/fonts'))
r=subprocess.run([str(qt/'bin/qmltestrunner.exe'),'-input',str(OUT/'tst_mix.qml'),'-o',str(OUT/'results.txt')+',txt'],cwd=OUT,env=env,capture_output=True,text=True,timeout=50,creationflags=subprocess.CREATE_NO_WINDOW)
(OUT/'stderr.log').write_text(r.stderr,encoding='utf-8');print((OUT/'results.txt').read_text(encoding='utf-8'));print(r.stderr[-4000:] if r.returncode else '');raise SystemExit(r.returncode)
