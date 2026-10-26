import os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'out/mix-ui';OUT.mkdir(parents=True,exist_ok=True)
module=OUT/'MixHarness';module.mkdir(exist_ok=True)
(module/'qmldir').write_text('module MixHarness\nsingleton App 1.0 App.qml\nsingleton Theme 1.0 Theme.qml\nsingleton Icons 1.0 Icons.qml\nsingleton Nav 1.0 Nav.qml\nsingleton Playlists 1.0 Playlists.qml\n')
for n in ['Theme','Icons']:shutil.copyfile(ROOT/f'qml/{n}.qml',module/f'{n}.qml')
(module/'Nav.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject{property bool canBack:false;property bool canForward:false;property var current:({page:"home"});function back(){}function forward(){}property var me:({id:"user"});function toast(t){}function go(a,b){}function likedSongsUri(){return ""}}')
(module/'Playlists.qml').write_text('pragma Singleton\nimport QtQuick\nQtObject{property var editable:[];function createWithTracks(a){}}')
themes_json=(ROOT/'resources/themes.json').read_text(encoding='utf-8')
(module/'App.qml').write_text('''pragma Singleton
import QtQuick
QtObject {
 property QtObject librespot:QtObject{property int state:1;property string statusText:"Test engine";property int bitrate:320;function restart(){}function login(){}}
 property string version:"test"
 property QtObject theme:QtObject{property string themeId:"tonal";property var themes:('''+themes_json+''').themes;property var current:themes.filter(t=>t.id===themeId)[0]}
 function dominantColor(url,callback){callback("#344455")}
 property QtObject auth:QtObject{property bool authenticated:true;property string userId:"user"}
 property QtObject music:QtObject{property var homeAlbums:[];function requestHomeAlbums(){}signal changed();function summary(k,v){return {ms:0,plays:0,tracks:[]}}}
 property QtObject player:QtObject{property int volume:60;function setVolume(v){volume=Math.max(0,Math.min(100,v))}property string contextUri:"";property string trackUri:"";property bool hasTrack:false;property string repeatMode:"off";property int repeatCycles:0;function cycleRepeat(){repeatCycles++;repeatMode=repeatMode==="off"?"context":repeatMode==="context"?"track":"off"}property bool isPlaying:false;property bool shuffle:false;property var queue:[{uri:"spotify:track:next",name:"Next song",artists:"Next artist",durationMs:180000}];property bool delayQueue:false;property int refreshes:0;signal queueRefreshFinished(int status);function refreshQueue(){refreshes++;if(!delayQueue){queueChanged();queueRefreshFinished(200)}}signal playbackRequested();function togglePlay(){}function playContext(a,b,c){}function playUris(a,b){}}
 property QtObject listeningMix:QtObject{property bool busy:false;property string status:"";property var received:({});property var previewQueue:[];property var committedRows:[];property bool discoveryEnabled:false;property bool randomEnabled:false;property bool enrichmentEnabled:false;property string enrichmentStatus:"";function queueFor(uri){return committedRows}property var applied:null;function applyModes(rows,shake,random,discovery){applied={rows:rows,shake:shake,random:random,discovery:discovery}}property int appliedPosition:-1;signal changed();signal previewReady();function preview(rows,index,random,discovery,album){received={rows:rows,index:index,random:random,discover:discovery,album:album};previewQueue=rows.slice();previewReady();changed()}function applyPreview(position){appliedPosition=position}function cancel(){}function order(n,seed){let a=[];for(let i=1;i<n;i++)a.push(i);a.push(0);return a}function play(rows,index,random,discover,album){received={rows:rows,index:index,random:random,discover:discover,album:album}}}
 property QtObject api:QtObject{
  property int writes:0
  function call(method,path,query,body,callback){if(method!=="GET")writes++;let items=[];for(let i=0;i<6;i++)items.push({uri:"spotify:track:fixture"+i,id:"fixture"+i,name:"Fixture song "+i,duration_ms:180000,artists:[{id:"artist",name:"Test Band"}],album:{id:"album",name:"Test Album",images:[]}});if(path.indexOf("/albums/")===0)callback(200,{id:"album",uri:"spotify:album:album",name:"Test Album",release_date:"1994-01-01",artists:[{id:"artist",name:"Test Band"}],images:[],total_tracks:6,tracks:{items:items,next:null}});else callback(200,{items:[],next:null});}
 }
}
''',encoding='utf-8')
for folder in ['qml/components','qml/companion']:
 for p in (ROOT/folder).glob('*.qml'):
  text=p.read_text(encoding='utf-8');text=text.replace('import QtQuick','import MixHarness 1.0\nimport QtQuick',1);(OUT/p.name).write_text(text,encoding='utf-8')
(OUT/'DevicePopup.qml').write_text('import MixHarness 1.0'+chr(10)+(ROOT/'qml/panels/DevicePopup.qml').read_text(encoding='utf-8'),encoding='utf-8')
top=(ROOT/'qml/panels/TopBar.qml').read_text(encoding='utf-8').replace('import "qrc:/companion/qml/companion" as Personal','import "." as Personal')
for key,value in {'Ready':'1','Starting':'2','NeedsLogin':'3','NotFound':'4','Crashed':'5'}.items():top=top.replace('Librespot.'+key,value)
(OUT/'TopBar.qml').write_text('import MixHarness 1.0\n'+top,encoding='utf-8')
bar=(ROOT/'qml/panels/PlayerBar.qml').read_text(encoding='utf-8').replace('import "qrc:/companion/qml/companion" as Personal','import "." as Personal');(OUT/'PlayerBar.qml').write_text('import MixHarness 1.0'+chr(10)+bar,encoding='utf-8')
text=(ROOT/'qml/pages/CollectionPage.qml').read_text(encoding='utf-8').replace('import "qrc:/companion/qml/companion" as Personal','import "." as Personal');(OUT/'CollectionPage.qml').write_text('import MixHarness 1.0\n'+text,encoding='utf-8')
(OUT/'HomePage.qml').write_text('import MixHarness 1.0\n'+(ROOT/'qml/pages/HomePage.qml').read_text(encoding='utf-8'),encoding='utf-8')
(OUT/'tst_mix.qml').write_text('''import QtQuick
import QtQuick.Controls
import QtTest
import MixHarness 1.0
Pane {width:1000;height:800;padding:0;palette.button:"#263440";palette.buttonText:"#f5f5f5";background:Rectangle{color:"#121212"}
 QtObject{id:personal;property var current:({provider:"Spotify",id:"current",uri:"spotify:track:current",title:"Current song",artist:"Current artist",durationMs:180000});property var favorites:[];property var playlists:[];property var lines:[];property bool busy:false;property string background:"Sem animação";property string photo:"";property var lyricStyle:({});property bool syncEnabled:true;property string lyricStatus:"";property int syncOffset:0;property bool historyEnabled:true;property int requestedDays:-1;property var storageInfo:({databaseBytes:2048,lastBackupBytes:1024});property QtObject stories:QtObject{property var state:({ready:false,busy:false,summary:"",modelBytes:0,cacheBytes:0,status:"Synthetic UI fixture"});function cancel(){}};property var statisticsReport:({rows:[],recommendations:[]});signal statisticsChanged();function requestStatistics(d,k){requestedDays=d;statisticsChanged()}function insights(d){return {themes:[],artists:[],albums:[],genres:[]}}signal lyricsChanged();signal changed();function refreshMetadata(){}function statistics(d,k){return {rows:[],recommendations:[]}}function lineAt(p){return -1}function fetchLyrics(){}function translate(a){}function summary(k,v){return {}}function playlistTracks(k){return []}function liked(t){return false}property var seeks:[];function seek(ms){seeks=seeks.concat([ms])}}
 TopBar{id:globalHeader;z:100;width:1000;height:72}
 SignalSpy{id:statsSpy;target:globalHeader;signalName:"statisticsRequested"}
 SignalSpy{id:backupSpy;target:globalHeader;signalName:"backupRequested"}
 QtObject{id:jam;property bool busy:false;property string status:"Synthetic Jam fixture";property var session:({});property int reads:0;signal notice(string text);function refresh(){reads++}function start(){}function leave(){}function copyInvite(){}function join(value){}}
 JamDialog{id:jamDialog;controller:jam}
 JamToast{id:jamToast;controller:jam;width:380;z:1000}
 MusicPanel{id:personalPanel;controller:personal;audioEngine:null;mixController:App.listeningMix;transportPlayer:App.player;position:12345;themeTokens:Theme.palette}
 SeekBar{id:themedSeek;x:20;y:760;width:300;value:0.5}
 ThemeDialog{id:themeDialog}
 PlayerBar{id:playerBar;x:0;y:700;z:10;width:1000;height:80}
 CollectionPage{id:page;anchors.fill:parent;route:({page:"album",params:{id:"album"}})}
 HomePage{id:home;visible:false;width:900;height:700}
 TestCase{name:"CollectionMix";when:windowShown
  function captureSong(item,file){let done=false;item.grabToImage(function(image){image.saveToFile(file);done=true});tryVerify(function(){return done},3000)}
  function test_jam_dialog_and_bounded_plain_text_notices(){
   let before=jam.reads;jamDialog.present();compare(jam.reads,before+1);verify(jamDialog.visible);jamDialog.close();
   jam.notice("<b>Synthetic participant</b> entrou no Jam");compare(jamToast.message,"<b>Synthetic participant</b> entrou no Jam");verify(jamToast.showing);
   for(let i=0;i<20;i++)jam.notice("Synthetic change "+i);verify(jamToast.pending.length<=8);
   jamToast.message="";jamToast.pending=[];jamToast.showing=false;
  }
  function test_home_uses_local_album_evidence(){
   const first={kind:"album",id:"first",uri:"spotify:album:first",title:"Recently heard",listenedMs:1000,lastAt:2000};
   const favorite={kind:"album",id:"favorite",uri:"spotify:album:favorite",title:"Most heard",listenedMs:9000,lastAt:1000};
   App.music.homeAlbums=[first,favorite];compare(home.recent.length,2);compare(home.recent[0].title,"Recently heard");compare(home.favoriteAlbums[0].title,"Most heard");
   home.serviceAlbums=[first,{kind:"album",id:"saved",uri:"spotify:album:saved",title:"Saved album"}];compare(home.savedAlbums.length,3);
   App.music.homeAlbums=[];home.serviceAlbums=[];
  }
  function test_global_statistics_backup_and_all_time_period(){
   let stats=findChild(globalHeader,"globalStatistics"),backup=findChild(globalHeader,"globalBackup");verify(stats!==null&&backup!==null);verify(stats.visible&&backup.visible);let before=statsSpy.count;mouseClick(stats);compare(statsSpy.count,before+1);before=backupSpy.count;mouseClick(backup);compare(backupSpy.count,before+1);
   personalPanel.section=1;personalPanel.open();let period=findChild(personalPanel,"statisticsPeriod");compare(period.count,6);period.currentIndex=5;period.activated(5);compare(personal.requestedDays,0);personalPanel.showSong();verify(stats.visible&&backup.visible);let point=stats.mapToItem(globalHeader,0,0);verify(point.x>=0&&point.x+stats.width<=globalHeader.width);personalPanel.close();period.currentIndex=0;
  }
  function test_song_pages_fill_viewport_and_snap(){
   personalPanel.showSong();let pages=findChild(personalPanel,"songPages");verify(pages!==null);compare(pages.count,3);pages.currentIndex=0;wait(300);
   compare(Math.round(pages.contentY),0);for(let i=0;i<3;i++)compare(Math.round(pages.model.get(i).height),Math.round(pages.height));
   mouseWheel(pages,pages.width/2,pages.height/2,0,-600);wait(800);compare(pages.currentIndex,1);compare(Math.round(pages.contentY),Math.round(pages.height));
   captureSong(findChild(personalPanel,"songPanelBody"),"song-page-lyrics-0140.png");
   pages.currentIndex=2;wait(300);compare(Math.round(pages.contentY),Math.round(2*pages.height));
   pages.currentIndex=0;wait(300);console.log("SONG_GEOMETRY",personalPanel.height,personalPanel.contentItem.height,pages.height,pages.model.get(0).height,findChild(personalPanel,"songControls").y,findChild(personalPanel,"songControls").height);captureSong(findChild(personalPanel,"songPanelBody"),"song-page-music-0140.png");personalPanel.close();
  }
  function test_share_freezes_identity_and_validates_link(){
   personalPanel.showSong();let share=findChild(personalPanel,"songShareDialog");verify(share!==null);
   share.present({uri:"spotify:track:0123456789ABCDEFGHIJKL",title:"Música de teste · dados sintéticos",artist:"Artista de teste"});
   compare(share.link,"https://open.spotify.com/track/0123456789ABCDEFGHIJKL");let old=personal.current;personal.current=Object.assign({},old,{title:"Outra música"});compare(share.track.title,"Música de teste · dados sintéticos");
   for(let i=0;i<3;i++){share.style=i;wait(120);captureSong(findChild(share,"sharePreview"),"share-style-"+i+"-0140.png")}
   share.track={uri:"spotify:artist:0123456789ABCDEFGHIJKL"};compare(share.link,"");share.close();personal.current=old;personalPanel.close();
  }
  function test_discovery_waits_for_fresh_queue(){
   personalPanel.songQueueSource=[];personalPanel.queueAnchor="";App.player.queue=[];App.player.delayQueue=true;App.listeningMix.previewQueue=[];
   personalPanel.showSong();personalPanel.discoveryPlayback=true;let before=App.player.refreshes;personalPanel.remixSong(false);
   compare(App.player.refreshes,before+1);compare(App.listeningMix.previewQueue.length,0);
   App.player.queue=[{uri:"spotify:track:next",name:"Next song",artists:"Next artist",artistId:"artist123",albumId:"album123",album:"Next Album",year:1994,durationMs:180000}];App.player.queueRefreshFinished(200);
   compare(App.listeningMix.previewQueue.length,2);compare(App.listeningMix.previewQueue[1].artistId,"artist123");compare(App.listeningMix.previewQueue[1].year,1994);
   findChild(personalPanel,"songQueueDialog").close();personalPanel.close();App.player.delayQueue=false;personalPanel.discoveryPlayback=false;
  }
  function test_metadata_refresh_keeps_queue_request(){
   personalPanel.songQueueSource=[];personalPanel.queueEdited=false;App.player.delayQueue=true;App.listeningMix.previewQueue=[];
   personalPanel.showSong();personalPanel.remixSong(false);verify(personalPanel.queueLoading);
   let original=personal.current;personal.current=Object.assign({},original,{album:"Refreshed metadata"});
   verify(personalPanel.queueLoading);App.player.queueRefreshFinished(200);compare(App.listeningMix.previewQueue.length,2);
   findChild(personalPanel,"songQueueDialog").close();personalPanel.close();personal.current=original;App.player.delayQueue=false;
  }
  function test_queue_failure_finishes_without_applying(){
   personalPanel.songQueueSource=[];personalPanel.queueEdited=false;App.player.delayQueue=true;App.listeningMix.previewQueue=[];
   personalPanel.showSong();personalPanel.remixSong(false);verify(personalPanel.queueLoading);
   App.player.queueRefreshFinished(403);verify(!personalPanel.queueLoading);verify(!personalPanel.queueEdited);verify(personalPanel.queueNotice.indexOf("403")>=0);compare(App.listeningMix.previewQueue.length,0);
   findChild(personalPanel,"songQueueDialog").close();personalPanel.close();App.player.delayQueue=false;
  }
  function test_modes_preserve_committed_discoveries(){
   let mix=App.listeningMix;mix.committedRows=[personal.current,{uri:"spotify:track:source-next",title:"Source next"},{uri:"spotify:track:outside",title:"Accepted discovery",mixExternal:true}];mix.discoveryEnabled=true;
   personalPanel.showSong();tryCompare(personalPanel,"visible",true);personalPanel.openQueue();compare(personalPanel.queuedRows.length,3);verify(personalPanel.discoveryPlayback);
   personalPanel.changeSongMode(0);verify(mix.received.discover);compare(mix.received.rows.filter(t=>t.mixExternal).length,1);
   personalPanel.changeSongMode(1);verify(mix.received.discover);verify(mix.received.random);compare(mix.received.rows.filter(t=>t.mixExternal).length,1);
   personalPanel.changeSongMode(2);verify(!mix.received.discover);verify(mix.received.random);
   // Closing without Apply must restart the next edit from the committed queue.
   let dialog=findChild(personalPanel,"songQueueDialog");dialog.close();tryCompare(dialog,"visible",false);personalPanel.queuedRows=[personal.current];
   personalPanel.changeSongMode(0);verify(mix.received.discover);compare(mix.received.rows.filter(t=>t.mixExternal).length,1);
   findChild(personalPanel,"songQueueDialog").close();personalPanel.close();mix.committedRows=[];mix.discoveryEnabled=false;mix.randomEnabled=false;personalPanel.discoveryPlayback=false;personalPanel.randomPlayback=false;
  }
  function test_song_queue_preview(){personalPanel.showSong();tryCompare(personalPanel,"visible",true);let button=findChild(personalPanel,"songQueueButton");verify(button!==null);verify(button.visible);personalPanel.openQueue();compare(personalPanel.queuedRows.length,2);compare(personalPanel.queuedRows[0].title,"Current song");let dialog=findChild(personalPanel,"songQueueDialog");verify(dialog.visible);personalPanel.remixSong(true);compare(App.listeningMix.previewQueue.length,2);compare(App.listeningMix.appliedPosition,-1);App.listeningMix.applyPreview(personalPanel.position);compare(App.listeningMix.appliedPosition,12345);grabImage(dialog.contentItem).save("song-queue.png");dialog.close();personalPanel.close()}
  function test_track_switch_cancels_queue_request(){
   personalPanel.songQueueSource=[];personalPanel.queueEdited=false;App.player.delayQueue=true;App.listeningMix.previewQueue=[];
   personalPanel.showSong();personalPanel.remixSong(false);verify(personalPanel.queueLoading);
   let original=personal.current;personal.current=Object.assign({},original,{uri:"spotify:track:different"});
   verify(!personalPanel.queueLoading);verify(personalPanel.queueNotice.indexOf("mudou")>=0);App.player.queueRefreshFinished(200);compare(App.listeningMix.previewQueue.length,0);
   findChild(personalPanel,"songQueueDialog").close();personalPanel.close();personal.current=original;App.player.delayQueue=false;
  }
  function test_visual_order_and_playback_are_independent(){
   tryCompare(page,"totalTracks",6);let original=page.mixRows();compare(original.length,6);compare(original[0].uri,"spotify:track:fixture0");compare(original[0].artist,"Test Band");compare(original[0].year,1994)
   page.shakeList();verify(page.listShaken);compare(page.mixRows()[0].uri,"spotify:track:fixture1");page.restoreOrder();verify(!page.listShaken);compare(JSON.stringify(page.mixRows()),JSON.stringify(original));compare(App.api.writes,0)
   page.randomPlayback=true;page.discoveryPlayback=true;page.playFrom(2);compare(App.listeningMix.received.index,2);verify(App.listeningMix.received.random);verify(App.listeningMix.received.discover);compare(App.listeningMix.received.rows.length,6);compare(App.api.writes,0)
   wait(100);grabImage(page).save("collection-modes.png")
  }
  function test_song_seek_waits_for_release(){
   personalPanel.showSong();tryCompare(personalPanel,"visible",true);let s=findChild(personalPanel,"songSeek");verify(s!==null);tryCompare(s,"visible",true);
   personal.seeks=[];let y=s.height/2;mousePress(s,s.width*0.25,y);mouseMove(s,s.width*0.5,y);mouseMove(s,s.width*0.75,y);
   personalPanel.position=1000;compare(personal.seeks.length,0);verify(s.value>s.to*0.6);
   mouseRelease(s,s.width*0.75,y);compare(personal.seeks.length,1);verify(Math.abs(personal.seeks[0]-s.to*0.75)<s.to*0.06);
   personalPanel.position=personal.seeks[0];compare(s.value,personal.seeks[0]);personalPanel.position=12345;tryCompare(s,"value",12345);personalPanel.close()
  }
  function test_theme_switch_restyles_seekbar(){
   compare(Theme.themes.length,6);compare(themedSeek.style.thumbShape,"circle");verify(!themedSeek.thumbVisible);
   App.theme.themeId="solar";compare(Theme.themeId,"solar");verify(Qt.colorEqual(Theme.accent,"#ff8a4c"));compare(themedSeek.style.thumbShape,"pill");verify(themedSeek.thumbVisible);compare(themedSeek.barHeight,5);
   App.theme.themeId="tonal";verify(Qt.colorEqual(Theme.accent,"#0d80bf"));compare(themedSeek.style.thumbShape,"circle")
  }
  function test_theme_dialog_applies_choice(){
   themeDialog.open();tryCompare(themeDialog,"opened",true);let card=findChild(themeDialog.contentItem,"theme_neon");verify(card!==null);
   mouseClick(card);compare(App.theme.themeId,"neon");verify(Theme.seek.glow);verify(card.chosen);
   wait(80);let scene=themedSeek.Window.window.contentItem;grabImage(scene).save("theme-dialog-neon.png");themeDialog.close();App.theme.themeId="solar";wait(80);grabImage(scene).save("collection-solar.png");App.theme.themeId="neon";
   personalPanel.showSong();tryCompare(personalPanel,"visible",true);let s=findChild(personalPanel,"songSeek");compare(s.look.thumbShape,"circle");verify(s.look.glow);wait(80);grabImage(themedSeek.Window.window.contentItem).save("song-neon.png");personalPanel.close();
   themeDialog.close();App.theme.themeId="tonal"
  }
  function test_song_controls_apply_directly(){
   let mix=App.listeningMix;personalPanel.showSong();tryCompare(personalPanel,"visible",true);
   let row=findChild(personalPanel,"songControls");verify(row!==null);
   for(let name of ["songShake","songRandom","songRepeat","songDiscovery","songQueueButton"])verify(findChild(personalPanel,name).visible,name);
   mix.applied=null;mix.committedRows=[personal.current,{uri:"spotify:track:x",title:"X"},{uri:"spotify:track:y",title:"Y"}];
   mouseClick(findChild(personalPanel,"songShake"));verify(mix.applied!==null);verify(mix.applied.shake);compare(mix.applied.rows.length,3);compare(mix.applied.rows[0].uri,"spotify:track:current");verify(!mix.applied.discovery);
   mix.applied=null;mix.committedRows=[];let refreshes=App.player.refreshes;
   mouseClick(findChild(personalPanel,"songDiscovery"));compare(App.player.refreshes,refreshes+1);verify(mix.applied!==null);verify(mix.applied.discovery);verify(!mix.applied.shake);compare(mix.applied.rows[0].uri,"spotify:track:current");compare(mix.applied.rows[1].uri,"spotify:track:next");
   mix.applied=null;mouseClick(findChild(personalPanel,"songRandom"));verify(mix.applied.random);
   let repeat=findChild(personalPanel,"songRepeat");App.player.repeatMode="off";let cycles=App.player.repeatCycles;mouseClick(repeat);compare(App.player.repeatCycles,cycles+1);compare(App.player.repeatMode,"context");verify(repeat.filled);
   mouseClick(repeat);compare(repeat.symbol,"repeat1");wait(300);let rp=repeat.mapToItem(null,0,0);console.log("REPEAT_AT "+Math.round(rp.x)+" "+Math.round(rp.y)+" "+Math.round(repeat.width)+" "+Math.round(repeat.height));grabImage(themedSeek.Window.window.contentItem).save("repeat-one-window.png");mouseClick(repeat);verify(!repeat.filled);
   wait(80);grabImage(themedSeek.Window.window.contentItem).save("song-controls.png");personalPanel.close()
  }
  function test_collection_modes_apply_to_playing_queue(){
   let mix=App.listeningMix;mix.applied=null;tryCompare(page,"totalTracks",6);
   App.player.hasTrack=true;App.player.trackUri="spotify:track:fixture2";
   page.randomPlayback=true;verify(page.applyToPlaying());verify(mix.applied!==null);compare(mix.applied.rows[0].uri,"spotify:track:fixture2");compare(mix.applied.rows.length,6);verify(mix.applied.random);
   App.player.trackUri="spotify:track:elsewhere";mix.applied=null;verify(!page.applyToPlaying());compare(mix.applied,null);
   App.player.hasTrack=false;App.player.trackUri="";page.randomPlayback=false
  }
  function test_volume_drag_keeps_pointer_and_endpoints(){
   let s=findChild(playerBar,"volumeSlider");verify(s!==null);App.player.volume=60;
   let y=s.height/2;mousePress(s,s.width*0.25,y);compare(App.player.volume,25);
   mouseMove(s,s.width*0.8,y);verify(Math.abs(App.player.volume-80)<=1);
   App.player.volume=10;verify(Math.abs(s.shownValue-0.8)<0.02);
   mouseRelease(s,s.width*0.8,y);verify(Math.abs(App.player.volume-80)<=1);verify(Math.abs(s.shownValue-0.8)<0.02);
   mouseClick(s,0,y);compare(App.player.volume,0);mouseClick(s,s.width-1,y);compare(App.player.volume,100);
  }
  function test_volume_wheel_respects_delta_and_horizontal(){
   let s=findChild(playerBar,"volumeSlider");App.player.volume=50;s.wheelRemainder=0;s.wheelStamp=0;
   mouseWheel(s,s.width/2,s.height/2,0,120);compare(App.player.volume,52);
   mouseWheel(s,s.width/2,s.height/2,0,240);compare(App.player.volume,56);
   mouseWheel(s,s.width/2,s.height/2,0,-120);compare(App.player.volume,54);
   mouseWheel(s,s.width/2,s.height/2,120,0);compare(App.player.volume,54);
   for(let i=0;i<4;i++)mouseWheel(s,s.width/2,s.height/2,0,30);
   compare(App.player.volume,56);App.player.volume=100;mouseWheel(s,s.width/2,s.height/2,0,120);compare(App.player.volume,100);
  }
  function test_mute_restores_last_volume(){
   App.player.volume=83;let mute=findChild(playerBar,"muteButton");verify(mute!==null);
   mouseClick(mute);compare(App.player.volume,0);mouseClick(mute);compare(App.player.volume,83);
  }
  function test_player_bar_has_all_controls(){
   let mix=App.listeningMix;let row=findChild(playerBar,"transportControls");verify(row!==null);
   let names=[];for(let i=0;i<row.children.length;i++)names.push(row.children[i].objectName||row.children[i].toString().split("_")[0]);
   compare(names[0],"shakeButton");compare(names[1],"randomButton");compare(names[5],"repeatButton");compare(names[6],"discoveryButton");
   App.player.hasTrack=true;App.player.trackUri="spotify:track:current";mix.committedRows=[personal.current,{uri:"spotify:track:x"},{uri:"spotify:track:y"}];mix.applied=null;
   mouseClick(findChild(playerBar,"shakeButton"));verify(mix.applied!==null&&mix.applied.shake);compare(mix.applied.rows.length,3);
   mix.applied=null;mouseClick(findChild(playerBar,"discoveryButton"));verify(mix.applied.discovery&&!mix.applied.shake);
   let cycles=App.player.repeatCycles;mouseClick(findChild(playerBar,"repeatButton"));compare(App.player.repeatCycles,cycles+1);
   wait(80);grabImage(playerBar).save("player-bar-controls.png");
   App.player.hasTrack=false;App.player.trackUri="";App.player.repeatMode="off";mix.committedRows=[]
  }
 }
}''',encoding='utf-8')
qt=Path(os.environ.get('QTDIR',ROOT/'out/tools/Qt/6.8.3/msvc2022_64'));env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QT_QUICK_CONTROLS_STYLE='Basic',QT_PLUGIN_PATH=str(qt/'plugins'),QML_IMPORT_PATH=str(OUT),QT_QPA_FONTDIR=str(ROOT/'out/text-playlist-ui/fonts'))
r=subprocess.run([str(qt/'bin/qmltestrunner.exe'),'-input',str(OUT/'tst_mix.qml'),'-o',str(OUT/'results.txt')+',txt'],cwd=OUT,env=env,capture_output=True,text=True,timeout=50,creationflags=subprocess.CREATE_NO_WINDOW)
(OUT/'stderr.log').write_text(r.stderr,encoding='utf-8');print((OUT/'results.txt').read_text(encoding='utf-8'));print(r.stderr[-4000:] if r.returncode else '');raise SystemExit(r.returncode)
