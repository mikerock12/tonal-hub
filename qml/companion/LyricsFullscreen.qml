import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: screen
    objectName:"lyricsFullscreenWindow"
    required property var controller
    property var verses: controller.lines
    property var metrics: ({})
    property bool playing: false
    property int position: 0
    property int selected: -1
    readonly property var style: controller.lyricStyle
    color: "#242a38"
    title: "Letras • Tonal"
    function fontFamily(value){return value==="Serif"?"Georgia":value==="Mono"?"Consolas":value==="Rounded"?"Trebuchet MS":"Segoe UI"}
    function reveal(){toolbar.visible=true;hide.restart()}
    function present(){showFullScreen();requestActivate();reveal()}
    function sync(){const n=controller.lineAt(position);if(n!==selected){selected=n;if(n>=0)lines.positionViewAtIndex(n,ListView.Center)}}
    onPositionChanged:sync()
    Connections{target:screen.controller;function onLyricsChanged(){screen.sync()}}
    onClosing:visible=false
    Image {anchors.fill:parent;source:screen.controller.background==="Foto"?screen.controller.photo:screen.controller.background==="Capa"?(screen.controller.current.image || ""):"";asynchronous:true;sourceSize.width:1600;sourceSize.height:1200;fillMode:Image.PreserveAspectCrop}
    LyricBackdrop {id:backdrop;anchors.fill:parent;mode:screen.controller.background;playing:screen.playing;active:screen.visible&&screen.active;metrics:screen.metrics}
    Rectangle {anchors.fill:parent;color:"#77202732"}
    ListView {
        id:lines;anchors.fill:parent;anchors.margins:48;model:screen.verses;clip:true;spacing:28;reuseItems:true;ScrollBar.vertical:ScrollBar{}
        header:Item{height:80}footer:Item{height:180}
        delegate:Column{
            required property var modelData;required property int index
            width:lines.width;spacing:8;opacity:screen.selected<0||screen.selected===index?1:.55
            StyledVerse{width:parent.width;text:modelData.text;appearance:screen.style;playing:screen.playing&&screen.visible}
            StyledVerse{width:parent.width;text:modelData.translation || "";visible:text.length>0;translation:true;appearance:screen.style;playing:screen.playing&&screen.visible}
            TapHandler{onTapped:{screen.controller.alignLyrics(index,screen.position);screen.reveal()}}

        }
    }
    TapHandler {gesturePolicy:TapHandler.DragThreshold;onTapped:toolbar.visible?toolbar.visible=false:screen.reveal()}
    Rectangle {
        id:toolbar;objectName:"lyricsFullscreenControls";anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;anchors.bottomMargin:24;width:Math.min(parent.width-48,670);height:74;color:"#ee171b24";radius:18
        RowLayout{anchors.fill:parent;anchors.margins:12
            ActionIcon{symbol:"close";description:"Sair da tela cheia";onClicked:screen.close()}
            ComboBox{Layout.fillWidth:true;model:backdrop.modes;currentIndex:Math.max(0,model.indexOf(screen.controller.background));onActivated:{screen.controller.background=currentText;screen.reveal()} onPressedChanged:if(pressed)hide.stop()}
            CheckBox{text:"Sincronia";checked:screen.controller.syncEnabled;onToggled:screen.controller.syncEnabled=checked}
            ActionIcon{symbol:"font";description:"Fontes e cores";onClicked:{hide.stop();appearance.open()}}
        }
    }
    Timer{id:hide;interval:3500;onTriggered:if(!appearance.visible)toolbar.visible=false}
    LyricStyleDialog{id:appearance;controller:screen.controller;onClosed:screen.reveal()}
    Shortcut{sequence:"Escape";onActivated:screen.close()}
}
