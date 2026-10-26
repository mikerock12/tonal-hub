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
    signal refreshRequested()
    property var refreshAction:null
    property bool refreshBusy:false
    readonly property var style: controller.lyricStyle
    color: "#242a38"
    title: "Letras • Tonal"
    function fontFamily(value){return value==="Serif"?"Georgia":value==="Mono"?"Consolas":value==="Rounded"?"Trebuchet MS":"Segoe UI"}
    function reveal(){toolbar.visible=true;hide.restart()}
    function verseItem(index){return lines.itemAtIndex(index)}
    function tapLine(index){if(!toolbar.visible){reveal();return}controller.alignLyrics(index,position);hide.stop();toolbar.visible=false}
    function present(){showFullScreen();requestActivate();hide.stop();toolbar.visible=false}
    function sync(){const n=controller.lineAt(position);if(n!==selected){selected=n;if(n>=0)lines.positionViewAtIndex(n,ListView.Center)}}
    onPositionChanged:sync()
    Connections{target:screen.controller;function onLyricsChanged(){screen.sync()}}
    onClosing:visible=false
    Image {anchors.fill:parent;source:screen.controller.background==="Foto"?screen.controller.photo:screen.controller.background==="Capa"?(screen.controller.current.image || ""):"";asynchronous:true;sourceSize.width:1600;sourceSize.height:1200;fillMode:Image.PreserveAspectCrop}
    LyricBackdrop {id:backdrop;anchors.fill:parent;mode:screen.controller.background;playing:screen.playing;active:screen.visible&&screen.active;metrics:screen.metrics}
    Rectangle {anchors.fill:parent;color:"#77202732"}
    MouseArea {anchors.fill:parent;onClicked:screen.reveal()}
    ListView {
        id:lines;anchors.fill:parent;anchors.margins:48;model:screen.verses;clip:true;spacing:28;reuseItems:true;ScrollBar.vertical:ScrollBar{}
        header:Item{height:80}footer:Item{height:180}
        delegate:Column{
            required property var modelData;required property int index
            objectName:"fullscreenVerse"+index
            width:lines.width;spacing:8;opacity:screen.selected<0||screen.selected===index?1:.55
            StyledVerse{width:parent.width;text:modelData.text;appearance:screen.style;playing:screen.playing&&screen.visible}
            StyledVerse{width:parent.width;text:modelData.translation || "";visible:text.length>0;translation:true;appearance:screen.style;playing:screen.playing&&screen.visible}
            TapHandler{onTapped:screen.tapLine(index)}

        }
    }

    Rectangle {
        id:toolbar;objectName:"lyricsFullscreenControls";anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;anchors.bottomMargin:24;width:Math.min(parent.width-48,670);height:74;color:"#ee171b24";radius:18
        RowLayout{anchors.fill:parent;anchors.margins:12
            ActionIcon{symbol:"close";description:"Sair da tela cheia";onClicked:screen.close()}
            ActionIcon{objectName:"fullscreenRefreshLyrics";symbol:"refresh";description:"Atualizar letra";enabled:!screen.controller.busy&&!screen.refreshBusy;onClicked:{screen.refreshRequested();if(screen.refreshAction)screen.refreshAction();else if(typeof screen.controller.reloadLyrics==="function")screen.controller.reloadLyrics();else screen.controller.fetchLyrics();screen.reveal()}}
            ComboBox{Layout.fillWidth:true;implicitHeight:42;font.pixelSize:14;palette.button:"#263440";palette.buttonText:"#f5f5f5";palette.text:"#f5f5f5";palette.base:"#171b24";palette.window:"#171b24";palette.windowText:"#f5f5f5";model:backdrop.modes;currentIndex:Math.max(0,model.indexOf(screen.controller.background));onActivated:{screen.controller.background=currentText;screen.reveal()} onPressedChanged:if(pressed)hide.stop()}
            CheckBox{text:"Sincronia";font.pixelSize:14;palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";checked:screen.controller.syncEnabled;onToggled:screen.controller.syncEnabled=checked}
            ActionIcon{symbol:"font";description:"Fontes e cores";onClicked:{hide.stop();appearance.open()}}
        }
    }
    Timer{id:hide;interval:3500;onTriggered:if(!appearance.visible)toolbar.visible=false}
    LyricStyleDialog{id:appearance;controller:screen.controller;onClosed:screen.reveal()}
    Shortcut{sequence:"Escape";onActivated:screen.close()}
}
