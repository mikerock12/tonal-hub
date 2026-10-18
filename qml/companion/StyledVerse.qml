import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Label {
    id: verse
    required property var appearance
    property bool translation: false
    property bool playing: false
    readonly property string prefix: translation?"translation":"lyric"
    readonly property string effect: appearance[prefix+"Effect"] || "Normal"
    readonly property color accent: appearance[prefix+"OutlineColor"] || "#101722"
    textFormat: Text.PlainText
    wrapMode: Text.Wrap
    color: appearance[prefix+"Color"] || "#F5F5F5"
    font.pixelSize: appearance[prefix+"Size"] || 26
    font.family: {const f=appearance[prefix+"Font"];return f==="Serif"?"Georgia":f==="Mono"?"Consolas":f==="Rounded"?"Trebuchet MS":"Segoe UI"}
    font.bold: !!appearance[prefix+"Bold"]
    font.italic: !!appearance[prefix+"Italic"]
    font.underline: !!appearance[prefix+"Underline"]
    style: effect==="Contorno"||effect==="Neon"?Text.Outline:Text.Normal
    styleColor: accent
    // Opt-in glow uses a small cached layer, with no per-frame blur animation.
    layer.enabled: visible&&(effect==="Brilho"||effect==="Neon")
    layer.effect: MultiEffect {shadowEnabled:true;shadowColor:verse.accent;shadowBlur:0.35;shadowHorizontalOffset:0;shadowVerticalOffset:0;shadowScale:1.03}
    SequentialAnimation on opacity {
        running:verse.visible&&verse.playing&&verse.effect==="Piscante"
        loops:Animation.Infinite
        NumberAnimation{to:0.4;duration:650}
        NumberAnimation{to:1;duration:650}
        onRunningChanged:if(!running)verse.opacity=1
    }
}
