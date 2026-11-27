import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "LyricColors.js" as LyricColors

Label {
    id: verse
    required property var appearance
    property bool translation: false
    property bool playing: false
    // Optional overrides used by the menus to preview an option before choosing it.
    property string fontOverride: ""
    property string effectOverride: ""
    readonly property string prefix: translation?"translation":"lyric"
    readonly property string effect: effectOverride || appearance[prefix+"Effect"] || "Normal"
    readonly property string fontName: fontOverride || appearance[prefix+"Font"] || "Sans"
    readonly property color accent: appearance[prefix+"OutlineColor"] || "#101722"
    readonly property color ink: appearance[prefix+"Color"] || "#F5F5F5"
    // The software renderer (graphics fallback) has no shader effects: those effects degrade to plain text, never to invisible text.
    readonly property bool shaders: GraphicsInfo.api!==GraphicsInfo.Software&&GraphicsInfo.api!==GraphicsInfo.Unknown
    readonly property bool gradientFill: effect==="Gradiente"&&shaders
    textFormat: Text.PlainText
    wrapMode: Text.Wrap
    // Vazado: hollow letters (outline in the text color); Gradiente: the fill comes from the masked gradient below.
    color: effect==="Vazado"||gradientFill?"transparent":ink
    font.pixelSize: appearance[prefix+"Size"] || 26
    font.family: LyricColors.family(fontName)
    font.capitalization: fontName==="Versalete"?Font.SmallCaps:Font.MixedCase
    font.weight: LyricColors.weight(fontName,!!appearance[prefix+"Bold"])
    font.italic: !!appearance[prefix+"Italic"]
    font.underline: !!appearance[prefix+"Underline"]
    style: effect==="Contorno"||effect==="Neon"||effect==="Contorno grosso"?Text.Outline:effect==="Vazado"?Text.Outline:effect==="Relevo"?Text.Raised:Text.Normal
    styleColor: effect==="Vazado"?ink:accent
    background: Rectangle {
        visible: verse.effect==="Marca-texto"
        color: Qt.rgba(verse.accent.r,verse.accent.g,verse.accent.b,0.7);radius:5
        x:-4;width:Math.min(parent.width+8,verse.contentWidth+8);height:verse.contentHeight
    }
    // Static, cached layers only when an effect needs them; no per-frame blur or shader animation.
    readonly property bool shadowed: effect==="Brilho"||effect==="Neon"||effect==="Sombra"||effect==="3D"||effect==="Contorno grosso"
    layer.enabled: visible&&shadowed&&shaders
    layer.effect: MultiEffect {
        shadowEnabled:true;shadowColor:verse.accent
        shadowBlur:verse.effect==="3D"?0:verse.effect==="Sombra"?0.3:verse.effect==="Contorno grosso"?0.12:verse.effect==="Brilho"?0.6:0.35
        shadowHorizontalOffset:verse.effect==="Sombra"?3:verse.effect==="3D"?4:0
        shadowVerticalOffset:verse.effect==="Sombra"?3:verse.effect==="3D"?4:0
        // No shadowScale: scaling grows from the item centre and shifted the glow of left-aligned lines in wide rows.
        shadowScale:1.0
        shadowOpacity:verse.effect==="Contorno grosso"?1.0:0.9
    }
    Text {
        id: maskText
        visible: false
        width: verse.width;text: verse.text;wrapMode: verse.wrapMode;font: verse.font;textFormat: Text.PlainText;color: "white"
        layer.enabled: verse.gradientFill&&verse.visible
    }
    Rectangle {
        anchors.fill: parent;visible: verse.gradientFill
        gradient: Gradient { GradientStop { position: 0; color: verse.ink } GradientStop { position: 1; color: verse.accent } }
        layer.enabled: visible
        layer.effect: MultiEffect { maskEnabled: true; maskSource: maskText; maskThresholdMin: 0.3; maskSpreadAtMin: 0.3 }
    }
    SequentialAnimation on opacity {
        running:verse.visible&&verse.playing&&verse.effect==="Piscante"
        loops:Animation.Infinite
        NumberAnimation{to:0.4;duration:650}
        NumberAnimation{to:1;duration:650}
        onRunningChanged:if(!running)verse.opacity=1
    }
}
