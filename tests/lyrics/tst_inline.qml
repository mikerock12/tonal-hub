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
        function test_manual_sync_toggle(){App.music.alignLyrics(1,15000);compare(App.music.syncOffset,-14000);App.music.syncEnabled=false;compare(App.music.lineAt(15000),-1);App.music.syncEnabled=true}
        function test_fullscreen_styles_and_controls() {
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
            full.close()
            tryCompare(full,"visible",false)
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
            verify(!button.enabled)
            App.music.lines = [{time:-1,text:"Hello world",translation:"Olá mundo"},
                               {time:-1,text:""}, {time:-1,text:"A synthetic verse",translation:"Um verso sintético"}]
            App.music.busy = false
            tryCompare(page, "hasTranslation", true)
            const translated = findChild(page, "lyricTranslationLine")
            verify(translated !== null)
            tryCompare(translated, "text", "Olá mundo")
            verify(translated.visible)
            grabImage(page).save("translated-page.png")
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
            verify(!button.enabled)
            App.music.current = ({uri:"spotify:track:next"})
            App.lyrics.lines = [{time:0,text:"Next synthetic song"}]
            App.lyrics.status = Lyrics.Synced
            App.lyrics.changed()
            tryCompare(App.music, "translateCalls", 2)
            compare(App.music.lines[0].text, "Next synthetic song")
        }
    }
}
