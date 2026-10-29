import QtQuick
import QtQuick.Window

Canvas {
    id: stage
    property string mode: "Aurora"
    property bool playing: false
    property bool active: visible
    property var metrics: ({})
    property real phase: 0
    readonly property var modes: ["Aurora","Ondas","Espectro","Capa","Foto","Estrelas","Chuva","Orbitas","Pulso","Grade","Halloween","Bruxas","Monstros","Vampiros","Sem animação"]
    onModeChanged: requestPaint()
    onPlayingChanged: requestPaint()
    Timer { interval:50; repeat:true; running:stage.active && stage.visible && (!stage.Window.window || stage.Window.window.active) && stage.playing && ["Capa","Foto","Sem animação"].indexOf(stage.mode)<0; onTriggered:{stage.phase+=.045;stage.requestPaint()} }
    onPaint: {
        const c=getContext("2d");c.reset();const w=width,h=height,p=phase
        c.strokeStyle="#62dfcf";c.fillStyle="#62dfcf";c.lineWidth=2
        if(mode==="Aurora")for(let i=0;i<3;i++){let x=w*(.5+.35*Math.sin(p*.4+i)),y=h*(.5+.3*Math.cos(p*.3+i)),g=c.createRadialGradient(x,y,0,x,y,Math.max(1,w*.65));g.addColorStop(0,["#185575","#302c5c","#155345"][i]);g.addColorStop(1,"transparent");c.fillStyle=g;c.fillRect(0,0,w,h)}
        if(mode==="Estrelas")for(let i=0;i<40;i++){c.globalAlpha=.35+.3*(1+Math.sin(p+i));c.beginPath();c.arc((i*73%101)*w/101,(i*47%103)*h/103,1+i%3,0,Math.PI*2);c.fill()}
        if(mode==="Chuva")for(let i=0;i<24;i++){let x=(i*73%101)*w/101,y=(i*.087+p*.2)%1*h;c.beginPath();c.moveTo(x,y);c.lineTo(x-w*.018,y+h*.045);c.stroke()}
        if(mode==="Orbitas")for(let i=0;i<4;i++){let r=Math.min(w,h)*(.12+i*.09);c.beginPath();c.arc(w/2,h/2,r,0,Math.PI*2);c.stroke();c.beginPath();c.arc(w/2+Math.cos(p/(i+1)+i)*r,h/2+Math.sin(p/(i+1)+i)*r,4,0,Math.PI*2);c.fill()}
        if(mode==="Pulso")for(let i=0;i<4;i++){let f=(p*.28+i*.25)%1;c.globalAlpha=.7*(1-f);c.beginPath();c.arc(w/2,h/2,Math.min(w,h)*f*.7,0,Math.PI*2);c.stroke()}
        if(mode==="Grade"){c.globalAlpha=.5;c.beginPath();for(let i=-6;i<=6;i++){c.moveTo(w/2+i*w/20,h*.25);c.lineTo(w/2+i*w/4,h)}for(let i=0;i<10;i++){let t=(i+p*.35)%10/10,y=h*(.25+.75*t*t);c.moveTo(0,y);c.lineTo(w,y)}c.stroke()}
        if (["Halloween","Bruxas","Monstros","Vampiros"].indexOf(mode)>=0) {
            c.fillStyle = mode === "Bruxas" ? "#302042" : mode === "Monstros" ? "#182b20" : "#2d121e"; c.fillRect(0,0,w,h)
            c.fillStyle = mode === "Monstros" ? "#a5d881" : "#ffad63"; c.globalAlpha=.5; c.beginPath(); c.arc(w*.78,h*.18,Math.min(w,h)*.09,0,Math.PI*2); c.fill(); c.globalAlpha=1
            c.fillStyle = "#09090f"; for(let i=0;i<9;i++){let x=(i*97%101)*w/101, y=h*(.15+(i*37%55)/100)+Math.sin(p+i)*3; c.beginPath(); c.moveTo(x-12,y); c.lineTo(x-4,y-4); c.lineTo(x,y); c.lineTo(x+4,y-4); c.lineTo(x+12,y); c.lineTo(x+4,y+3); c.lineTo(x,y+1); c.lineTo(x-4,y+3); c.closePath(); c.fill()}
            c.strokeStyle=mode === "Monstros"?"#658c63":"#b96556"; c.globalAlpha=.35; c.lineWidth=1; for(let i=0;i<7;i++){c.beginPath();c.moveTo(0,0);c.lineTo(w*.4*Math.cos(i*Math.PI/12),h*.4*Math.sin(i*Math.PI/12));c.stroke()}
        }
        const data=mode==="Ondas"?metrics.waveform:metrics.spectrum
        if((mode==="Ondas"||mode==="Espectro")&&data&&data.length&&playing){c.beginPath();for(let i=0;i<data.length;i++)if(mode==="Ondas"){let y=h*(.5+data[i]*.35);i?c.lineTo(i*w/(data.length-1),y):c.moveTo(0,y)}else c.fillRect(i*w/data.length,h*(1-data[i]*.8),w/data.length-2,h*data[i]*.8);c.stroke()}
    }
}
