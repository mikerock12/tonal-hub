import QtQuick
import QtQuick.Controls

AbstractButton {
    id: button
    property string symbol: "play"
    property string description: ""
    property bool filled: false
    property color ink: filled ? "#009bd6" : "#f5f5f5"
    implicitWidth: 44; implicitHeight: 44
    Accessible.name: description
    ToolTip.visible: hovered
    ToolTip.text: description
    background: Rectangle { radius: width/2; color: button.down ? "#475363" : button.hovered ? "#303641" : "transparent" }
    onSymbolChanged: drawing.requestPaint()
    onInkChanged: drawing.requestPaint()
    onFilledChanged: drawing.requestPaint()
    contentItem: Canvas {
        id: drawing
        onPaint: {
            const c=getContext("2d");c.reset();c.save();c.translate((width-24)/2,(height-24)/2)
            c.strokeStyle=button.ink;c.fillStyle=button.ink;c.lineWidth=1.8;c.lineCap="round";c.lineJoin="round";c.beginPath()
            const line=(a,b,x,y)=>{c.moveTo(a,b);c.lineTo(x,y)}
            switch(button.symbol){
            case "now":c.arc(12,12,9,0,Math.PI*2);c.moveTo(15,12);c.arc(12,12,3,0,Math.PI*2);break
            case "library":line(4,4,4,21);line(10,4,10,21);line(16,4,21,21);break
            case "chart":line(3,21,21,21);line(6,17,6,11);line(12,17,12,6);line(18,17,18,3);break
            case "lyrics":line(4,5,20,5);line(4,10,17,10);line(4,15,20,15);line(4,20,14,20);break
            case "save":line(12,3,12,16);line(7,11,12,16);line(12,16,17,11);line(4,18,4,21);line(4,21,20,21);line(20,21,20,18);break
            case "shake":line(3,5,15,5);line(3,12,11,12);line(3,19,15,19);line(17,4,21,8);line(21,8,17,12);line(21,13,17,17);line(17,17,21,21);break
            case "random":c.rect(3,3,18,18);c.fillRect(7,7,3,3);c.fillRect(11,11,3,3);c.fillRect(15,15,3,3);break
            case "discover":c.arc(12,12,9,0,Math.PI*2);c.moveTo(16,7);c.lineTo(14,14);c.lineTo(7,17);c.lineTo(10,10);c.closePath();break
            case "audio":for(let i=0;i<3;i++){let x=5+i*7,y=7+(i%2)*9;line(x,3,x,21);c.moveTo(x+2,y);c.arc(x,y,2,0,Math.PI*2)}break
            case "heart":c.moveTo(12,21);c.bezierCurveTo(-7,8,6,-2,12,7);c.bezierCurveTo(18,-2,31,8,12,21);if(button.filled)c.fill();break
            case "play":c.moveTo(7,4);c.lineTo(20,12);c.lineTo(7,20);c.closePath();c.fill();break
            case "pause":c.fillRect(6,4,4,16);c.fillRect(14,4,4,16);break
            case "prev":case "next":if(button.symbol==="prev"){c.translate(24,0);c.scale(-1,1)}c.moveTo(5,5);c.lineTo(16,12);c.lineTo(5,19);c.closePath();c.fill();c.fillRect(17,5,2,14);break
            case "refresh":c.arc(12,12,8,.5,5.7);line(20,3,20,9);line(20,9,14,9);break
            case "expand":for(let i=0;i<4;i++){c.save();c.translate(12,12);c.rotate(i*Math.PI/2);line(3,-8,8,-8);line(8,-8,8,-3);c.restore()}break
            case "close":line(5,5,19,19);line(19,5,5,19);break
            case "translate":line(3,6,14,6);line(8,3,8,6);line(12,6,6,16);line(5,9,12,15);line(13,21,18,10);line(18,10,23,21);line(15,17,21,17);break
            case "palette":c.arc(12,12,9,0,Math.PI*2);for(let i=0;i<3;i++){c.moveTo(8+i*5,9+i%2*5);c.arc(7+i*5,9+i%2*5,1,0,Math.PI*2)}break
            case "font":line(4,5,20,5);line(12,5,12,21);line(8,21,16,21);break
            case "plus":line(12,4,12,20);line(4,12,20,12);break
            }c.stroke();c.restore()
        }
    }
}
