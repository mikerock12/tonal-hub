import QtQuick
import QtTest
import LyricsHarness 1.0

Item {
    width: 850
    height: 700
    LyricsPage { id: page; anchors.fill: parent }
    ClimateDetail{id:climate;controller:App.music;parent:page}
    TestCase {
        name: "InlineLyricTranslation"
        when: windowShown
        function test_climate_cards(){climate.open();tryCompare(climate,"visible",true);compare(climate.report.artistCount,1);compare(climate.report.themes[0].label,"rebeldia");grabImage(page).save("climate-cards.png");climate.close()}
        function test_header_icons(){
            for(const name of ["refreshLyricsButton","translateLyricsButton","syncLyricsButton","exportSyncButton"])verify(findChild(page,name)!==null,name)
            const sync=findChild(page,"syncLyricsButton");const before=App.music.syncEnabled
            compare(sync.filled,before);mouseClick(sync);compare(App.music.syncEnabled,!before);compare(sync.filled,!before)
            mouseClick(sync);compare(App.music.syncEnabled,before)
            // Atualizar ignora o cache e pede nova tradução; o teste restaura a chave para os demais casos.
            const saved=page.requestedTranslation;const calls=App.lyrics.reloadCalls;mouseClick(findChild(page,"refreshLyricsButton"));compare(App.lyrics.reloadCalls,calls+1);compare(page.requestedTranslation,"");page.requestedTranslation=saved
        }
        // 0.18.2: fonts and effects are previewed in their menus; colors come from a 0–1000 rainbow rail with −/+.
        function test_style_dialog_previews_and_rail(){
            const dialog=findChild(page,"lyricStyleDialog");verify(dialog!==null);dialog.open();tryCompare(dialog,"opened",true)
            const root=dialog.contentItem,box=findChild(root,"lyricFontBox")
            verify(box!==null);verify(box.count>=10);const effects=findChild(root,"lyricEffectBox");verify(effects!==null);verify(effects.count>=10)
            compare(box.model.indexOf("Cursiva")>=0,true);compare(effects.model.indexOf("Vazado")>=0,true)
            const rail=findChild(root,"lyricColorRail"),plus=findChild(root,"lyricColorPlus"),minus=findChild(root,"lyricColorMinus"),value=()=>findChild(root,"lyricColorValue").text
            verify(rail!==null&&plus!==null&&minus!==null);compare(rail.from,0);compare(rail.to,1000)
            mouseClick(minus);verify(/^#[0-9A-F]{6}$/.test(App.music.lyricStyle.lyricColor))
            rail.value=0;rail.moved();compare(value(),"#FFFFFF · 0")
            mouseClick(plus);verify(App.music.lyricStyle.lyricColor!=="#FFFFFF");compare(value().indexOf("· 1")>0,true)
            rail.value=1000;rail.moved();compare(value(),"#000000 · 1000")
            rail.value=50;rail.moved();compare(value(),"#FF4040 · 50")
            rail.value=200;rail.moved();compare(value(),"#FF8000 · 200")
            rail.value=600;rail.moved();compare(value(),"#00FFFF · 600")
            rail.value=800;rail.moved();compare(value(),"#8000FF · 800")
            App.music.setLyricStyle("lyricColor","#F5F5F5");dialog.close();tryCompare(dialog,"visible",false)
        }
        function test_style_dialog_screenshots(){
            const dialog=findChild(page,"lyricStyleDialog");verify(dialog!==null);dialog.open();tryCompare(dialog,"opened",true);wait(150)
            grabImage(dialog.contentItem).save("style-dialog-0182.png")
            const fonts=findChild(dialog.contentItem,"lyricFontBox");fonts.popup.open();tryCompare(fonts.popup,"opened",true);wait(250)
            grabImage(page).save("style-fonts-0182.png");fonts.popup.close()
            const effects=findChild(dialog.contentItem,"lyricEffectBox");App.music.setLyricStyle("lyricOutlineColor","#00C9FF");effects.popup.open();tryCompare(effects.popup,"opened",true);wait(250)
            grabImage(page).save("style-effects-0182.png");effects.popup.close();dialog.close()
        }
        function test_manual_sync_toggle(){App.music.alignLyrics(1,15000);compare(App.music.syncOffset,-14000);App.music.syncEnabled=false;compare(App.music.lineAt(15000),-1);App.music.syncEnabled=true}
        function test_fullscreen_styles_and_controls() {
            const savedBusy=App.music.busy
            const full=findChild(page,"lyricsFullscreenWindow")
            verify(full!==null)
            full.present()
            tryCompare(full,"visibility",5) // QWindow::FullScreen
            const controls=findChild(full,"lyricsFullscreenControls")
            verify(controls!==null)
            verify(!controls.visible)
            App.music.setLyricStyle("lyricFont","Mono")
            App.music.setLyricStyle("translationColor","#FFBBD9")
            compare(full.style.lyricFont,"Mono")
            compare(full.style.translationColor,"#FFBBD9")
            App.music.background="Estrelas"
            wait(3700)
            compare(controls.visible,false)
            const refresh=findChild(full,"fullscreenRefreshLyrics")
            verify(refresh!==null)
            full.reveal();let calls=App.lyrics.reloadCalls
            App.music.busy=false;mouseClick(refresh);compare(App.lyrics.reloadCalls,calls+1)
            App.music.busy=true;verify(!refresh.enabled);App.music.busy=false
            controls.visible=false
            const offset=App.music.syncOffset
            const verse=full.verseItem(0)
            verify(verse!==null)
            mouseClick(verse,verse.width/2,verse.height/2) // hidden: reveal only
            compare(controls.visible,true)
            compare(App.music.syncOffset,offset)
            mouseClick(verse,verse.width/2,verse.height/2) // visible: align, then hide
            compare(App.music.syncOffset,-full.position)
            compare(controls.visible,false)
            mouseClick(verse,verse.width/2,verse.height/2) // hidden again: reveal without changing alignment
            compare(controls.visible,true)
            compare(App.music.syncOffset,-full.position)
            grabImage(full.contentItem).save("fullscreen-refresh-0151.png")
            full.close()
            tryCompare(full,"visible",false)
            App.music.busy=savedBusy
        }
        function test_translation_and_track_change() {
            const button = findChild(page, "translateLyricsButton")
            verify(button !== null)
            verify(button.visible)
            tryCompare(App.music, "lastLanguage", "auto")
            compare(App.music.translateCalls, 1)
            compare(App.music.lines[0].text, "Hello world")
            compare(App.music.lines.length, 3) // blank verse must not shift the translation
            verify(page.translationMatches)
            verify(button.filled)
            App.music.lines = [{time:-1,text:"Hello world",translation:"Olá mundo"},
                               {time:-1,text:""}, {time:-1,text:"A synthetic verse",translation:"Um verso sintético"}]
            App.music.busy = false
            tryCompare(page, "hasTranslation", true)
            const translated = findChild(page, "lyricTranslationLine")
            verify(translated !== null)
            tryCompare(translated, "text", "Olá mundo")
            verify(translated.visible)
            grabImage(page).save("translated-page.png")
            // Header toggle: white/off hides the translation, blue/on shows it again
            // without a new request when this exact lyric was already translated.
            page.requestedTranslation=App.player.trackUri+"|"+App.lyrics.lines.map(row=>row.time+":"+row.text).join("
")
            const callsBeforeToggle=App.music.translateCalls
            mouseClick(button)
            verify(!button.filled)
            tryCompare(translated, "visible", false)
            mouseClick(button)
            verify(button.filled)
            tryCompare(translated, "visible", true)
            compare(App.music.translateCalls, callsBeforeToggle)
            // Replacing the original version or changing tracks must hide stale translations.
            App.lyrics.lines = [{time:-1,text:"Another recording"}]
            tryCompare(page, "hasTranslation", false)
            App.lyrics.lines = [{time:-1,text:"Hello world"},{time:-1,text:""},{time:-1,text:"A synthetic verse"}]
            tryCompare(page, "hasTranslation", true)
            App.player.trackUri = "spotify:track:next"
            App.player.trackChanged()
            tryCompare(page, "hasTranslation", false)
            App.lyrics.status = Lyrics.Loading
            App.lyrics.lines = []
            App.lyrics.changed()
            App.music.current = ({uri:"spotify:track:next"})
            App.lyrics.lines = [{time:0,text:"Next synthetic song"}]
            App.lyrics.status = Lyrics.Synced
            App.lyrics.changed()
            tryCompare(App.music, "translateCalls", 2)
            compare(App.music.lines[0].text, "Next synthetic song")
        }
    }
}
