import QtQuick
import QtTest
import LyricsHarness 1.0

Item {
    width: 850
    height: 700
    LyricsPage { id: page; anchors.fill: parent }
    TestCase {
        name: "InlineLyricTranslation"
        when: windowShown
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
