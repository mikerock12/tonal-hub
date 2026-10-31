import QtQuick
import QtQuick.Window

// Bounded native Canvas: one visible surface, seven streams, at most 15 fps.
// Its clock never changes the seek value or sends playback/volume commands.
Canvas {
    id: blood
    property real progress: 0
    property real barHeight: 6
    property real thumbSize: 20
    property real trackInset: 0
    property bool showThumb: true
    property bool playing: false
    property bool animationAllowed: true
    property color fillColor: "#c52132"
    property color endColor: "#f04b3c"
    property real phase: 0
    readonly property bool animationRunning: visible && enabled && playing && animationAllowed && progress > 0 && (!Window.window || Window.window.active)
    implicitHeight: 44
    onProgressChanged: requestPaint()
    onBarHeightChanged: requestPaint()
    onThumbSizeChanged: requestPaint()
    onShowThumbChanged: requestPaint()
    onVisibleChanged: requestPaint()
    onFillColorChanged: requestPaint()
    onEndColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPhaseChanged: requestPaint()
    Timer { interval: 67; repeat: true; running: blood.animationRunning; onTriggered: blood.phase += .075 }
    function drop(c,x,y,size,opacity) {
        c.save();c.translate(x-size/2,y-size/2);c.globalAlpha=opacity
        c.beginPath();c.moveTo(size*.5,0)
        c.bezierCurveTo(size*.44,size*.25,size*.05,size*.52,size*.12,size*.76)
        c.bezierCurveTo(size*.21,size*1.06,size*.79,size*1.06,size*.88,size*.76)
        c.bezierCurveTo(size*.95,size*.52,size*.56,size*.25,size*.5,0);c.closePath()
        const g=c.createLinearGradient(0,0,size,size);g.addColorStop(0,"#ff6264");g.addColorStop(.32,endColor);g.addColorStop(.68,fillColor);g.addColorStop(1,"#4d040f")
        c.fillStyle=g;c.fill();c.strokeStyle="#5c0712";c.lineWidth=1.1;c.stroke()
        c.beginPath();c.ellipse(size*.23,size*.42,size*.13,size*.29);c.fillStyle="#ffe0d2";c.globalAlpha=opacity*.74;c.fill()
        c.beginPath();c.ellipse(size*.42,size*.81,size*.3,size*.055);c.fillStyle="#ff9b8d";c.globalAlpha=opacity*.48;c.fill()
        c.beginPath();c.arc(size*.67,size*.58,size*.065,0,Math.PI*2);c.fillStyle="#fff1df";c.globalAlpha=opacity*.7;c.fill();c.restore()
    }
    onPaint: {
        const c=getContext("2d");c.reset();c.translate(trackInset,0);const cy=height/2,amount=Math.max(0,Math.min(1,progress)),edge=(width-2*trackInset)*amount
        if(edge>0){
            const g=c.createLinearGradient(0,cy-barHeight/2,0,cy+barHeight/2)
            g.addColorStop(0,"#ff9a86");g.addColorStop(.16,"#ff3f45");g.addColorStop(.55,endColor);g.addColorStop(.82,fillColor);g.addColorStop(1,"#4d040f")
            c.fillStyle=g;c.beginPath();c.roundedRect(0,cy-barHeight/2,edge,barHeight,barHeight/2,barHeight/2);c.fill()
            for(let i=0;i<7;i++){
                const cycle=(phase*(.35+(i%3)*.08)+i*.173)%1,x=edge*(i+1)/8,neck=1.3+(i%3)*.5
                const length=3+(i%3)*2+Math.sin(Math.min(cycle,.8)/.8*Math.PI/2)*(5+i%2*3),top=cy+barHeight*.28,bottom=top+length
                c.fillStyle=fillColor;c.beginPath();c.moveTo(x-neck*1.5,top);c.bezierCurveTo(x-neck,top+2,x-neck,bottom-2,x,bottom);c.bezierCurveTo(x+neck,bottom-2,x+neck,top+2,x+neck*1.5,top);c.closePath();c.fill()
                c.strokeStyle="#f45e59";c.globalAlpha=.4;c.lineWidth=.65;c.beginPath();c.moveTo(x-neck*.5,top);c.lineTo(x-neck*.25,bottom-2);c.stroke();c.globalAlpha=1
                if(cycle>.76)drop(c,x,bottom+(cycle-.76)*22,3.5+(i%2),1-(cycle-.76)/.24)
            }
        }
        if(showThumb)drop(c,edge,cy+thumbSize/2,thumbSize,enabled?1:.45)
    }
}
