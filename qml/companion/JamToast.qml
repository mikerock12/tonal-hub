import QtQuick
import QtQuick.Controls

Rectangle {
    id:toast
    required property var controller
    property var pending:[]
    property string message:""
    property bool showing:false
    width:parent?parent.width:420
    height:label.implicitHeight+28
    color:"#ee183c4a";radius:14
    visible:message.length>0;opacity:showing?1:0
    transform:Translate {y:toast.showing?0:18;Behavior on y {NumberAnimation {duration:220}}}
    Behavior on opacity {NumberAnimation {duration:220}}
    function enqueue(value){const text=String(value).slice(0,600);if(!text||text===message||pending.indexOf(text)>=0)return;pending=pending.slice(-7).concat([text]);if(!message)advance()}
    function advance(){if(!pending.length){message="";return}message=pending[0];pending=pending.slice(1);showing=true;display.restart()}
    Connections {target:toast.controller;function onNotice(text){toast.enqueue(text)}}
    Label {id:label;anchors.fill:parent;anchors.margins:14;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:toast.message;color:"#f5f5f5";font.pixelSize:14}
    Timer {id:display;interval:3500;onTriggered:{toast.showing=false;fade.restart()}}
    Timer {id:fade;interval:240;onTriggered:toast.advance()}
}
