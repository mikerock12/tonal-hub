import QtQuick
import QtQuick.Window

// Decorative native vector animation. It never issues playback commands.
Canvas {
    id: ring
    property bool animationAllowed: true
    property real phase: 0
    readonly property bool animationRunning: visible && enabled && animationAllowed && Window.window && Window.window.active
    onPhaseChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Timer { interval: 67; repeat: true; running: ring.animationRunning; onTriggered: ring.phase += .16 }
    onPaint: {
        const c=getContext('2d');c.reset();const cx=width/2,cy=height/2,r=Math.min(width,height)*.46
        c.fillStyle='#130d15';c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.fill()
        c.lineWidth=Math.max(1,r*.1);c.strokeStyle='#3e2933';c.beginPath();c.arc(cx,cy,r*.96,0,Math.PI*2);c.stroke()
        c.lineWidth=Math.max(1,r*.045);c.strokeStyle='#ff873e';c.globalAlpha=.6+.4*(.5+.5*Math.sin(phase));c.beginPath();c.arc(cx,cy,r*.79,0,Math.PI*2);c.stroke()
        for(let i=0;i<8;i++){const a=i*2*Math.PI/8+.12,x=cx+Math.cos(a)*r*.84,y=cy+Math.sin(a)*r*.84;c.fillStyle=i%2?'#b22b25':'#fa7135';c.globalAlpha=.4+.5*(.5+.5*Math.sin(phase+i));c.beginPath();c.arc(x,y,Math.max(1,r*.035),0,Math.PI*2);c.fill()}
        c.globalAlpha=1;c.fillStyle='#9f1d23';for(let i=-1;i<=1;i++){const x=cx+i*r*.35,y=cy+r*.76,len=r*(.12+.08*(.5+.5*Math.sin(phase+i)));c.fillRect(x-r*.025,y,r*.05,len)}
    }
}
