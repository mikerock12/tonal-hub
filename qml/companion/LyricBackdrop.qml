import QtQuick
import QtQuick.Window

Canvas {
    id: stage
    property string mode: "Aurora"
    property bool playing: false
    property bool active: visible
    property var metrics: ({})
    property real phase: 0
    readonly property var modes: ["Aurora","Ondas","Espectro","Capa","Foto","Estrelas","Chuva","Orbitas","Pulso","Grade","Halloween","Bruxas","Monstros","Vampiros","Natal","Neve","Lareira","Sem animação"]
    onModeChanged: requestPaint()
    onPlayingChanged: requestPaint()
    Timer { interval:50; repeat:true; running:stage.active && stage.visible && (!stage.Window.window || stage.Window.window.active) && stage.playing && ["Capa","Foto","Sem animação"].indexOf(stage.mode)<0; onTriggered:{stage.phase+=.045;stage.requestPaint()} }

    function background(c,w,h,top,bottom) { const g=c.createLinearGradient(0,0,0,h);g.addColorStop(0,top);g.addColorStop(1,bottom);c.fillStyle=g;c.fillRect(0,0,w,h) }
    function moon(c,x,y,r,color) { c.fillStyle=color;c.globalAlpha=.09;c.beginPath();c.arc(x,y,r*1.65,0,Math.PI*2);c.fill();c.globalAlpha=.2;c.beginPath();c.arc(x,y,r*1.3,0,Math.PI*2);c.fill();c.globalAlpha=.85;c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fill();c.globalAlpha=.16;c.fillStyle="#332637";c.beginPath();c.arc(x-r*.25,y-r*.12,r*.19,0,Math.PI*2);c.arc(x+r*.3,y+r*.22,r*.13,0,Math.PI*2);c.fill();c.globalAlpha=1 }
    function bat(c,x,y,s,flap) { c.fillStyle="#08070c";c.beginPath();c.moveTo(x,y);c.bezierCurveTo(x-s*.4,y-s*flap,x-s*.8,y-s*flap,x-s,y+s*.1);c.lineTo(x-s*.55,y+s*.03);c.lineTo(x-s*.25,y+s*.35);c.lineTo(x,y+s*.1);c.lineTo(x+s*.25,y+s*.35);c.lineTo(x+s*.55,y+s*.03);c.lineTo(x+s,y+s*.1);c.bezierCurveTo(x+s*.8,y-s*flap,x+s*.4,y-s*flap,x,y);c.fill() }
    function pine(c,x,base,height,color,snow) { c.fillStyle=color;for(let i=0;i<4;i++){const y=base-height+i*height*.2,w=height*(.18+i*.07);c.beginPath();c.moveTo(x,y);c.lineTo(x-w,y+height*.42);c.lineTo(x+w,y+height*.42);c.closePath();c.fill()}c.fillRect(x-height*.025,base-height*.05,height*.05,height*.16);if(snow){c.strokeStyle="#dcecff";c.globalAlpha=.6;c.lineWidth=Math.max(1,height*.018);for(let i=1;i<4;i++){const y=base-height+i*height*.2+height*.25,w=height*(.1+i*.055);c.beginPath();c.moveTo(x-w,y);c.lineTo(x,y+height*.06);c.lineTo(x+w,y);c.stroke()}c.globalAlpha=1} }
    function pumpkin(c,x,y,r) { c.fillStyle="#37150c";c.fillRect(x-r*.08,y-r*1.05,r*.16,r*.35);const g=c.createRadialGradient(x-r*.3,y-r*.35,r*.1,x,y,r*1.2);g.addColorStop(0,"#f58b32");g.addColorStop(.65,"#b84318");g.addColorStop(1,"#5c1a12");c.fillStyle=g;for(let i=-2;i<=2;i++){c.beginPath();c.ellipse(x+i*r*.28-r*.45,y-r*.75,r*.9,r*1.5);c.fill()}c.fillStyle="#ffd064";c.globalAlpha=.85;c.beginPath();c.moveTo(x-r*.62,y-r*.25);c.lineTo(x-r*.25,y-r*.48);c.lineTo(x-r*.35,y-r*.02);c.closePath();c.moveTo(x+r*.62,y-r*.25);c.lineTo(x+r*.25,y-r*.48);c.lineTo(x+r*.35,y-r*.02);c.closePath();c.fill();c.beginPath();c.moveTo(x-r*.55,y+r*.25);c.lineTo(x-r*.25,y+r*.12);c.lineTo(x,y+r*.3);c.lineTo(x+r*.25,y+r*.12);c.lineTo(x+r*.55,y+r*.25);c.lineTo(x+r*.35,y+r*.55);c.lineTo(x-r*.35,y+r*.55);c.closePath();c.fill();c.globalAlpha=1 }
    function star(c,x,y,r,color) { c.fillStyle=color;c.beginPath();for(let i=0;i<10;i++){const a=-Math.PI/2+i*Math.PI/5,rr=i%2?r*.43:r,px=x+Math.cos(a)*rr,py=y+Math.sin(a)*rr;i?c.lineTo(px,py):c.moveTo(px,py)}c.closePath();c.fill() }
    function snowfall(c,w,h,p,count) { c.fillStyle="#edf7ff";for(let i=0;i<count;i++){const x=((i*83%107)+Math.sin(p*.3+i)*1.4)%107*w/107,y=(i*.071+p*.035*(1+i%3))%1*h,r=1+i%3*.55;c.globalAlpha=.25+(i%5)*.12;c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fill()}c.globalAlpha=1 }
    function grave(c,x,y,s,cross) { c.fillStyle="#24242d";if(cross){c.fillRect(x-s*.08,y-s*.8,s*.16,s*.8);c.fillRect(x-s*.3,y-s*.62,s*.6,s*.14)}else{c.beginPath();c.moveTo(x-s*.35,y);c.lineTo(x-s*.35,y-s*.55);c.arc(x,y-s*.55,s*.35,Math.PI,0);c.lineTo(x+s*.35,y);c.closePath();c.fill();c.strokeStyle="#48434d";c.lineWidth=1;c.beginPath();c.moveTo(x-s*.15,y-s*.42);c.lineTo(x+s*.15,y-s*.42);c.stroke()} }
    function flame(c,x,y,s,p) { const sway=Math.sin(p*5+x)*s*.12;c.fillStyle="#ff6a27";c.beginPath();c.moveTo(x-s*.45,y);c.bezierCurveTo(x-s*.65,y-s*.5,x+sway,y-s*1.2,x,y-s*1.55);c.bezierCurveTo(x+s*.7,y-s*.8,x+s*.55,y-s*.35,x+s*.4,y);c.closePath();c.fill();c.fillStyle="#ffe68a";c.beginPath();c.moveTo(x-s*.2,y);c.bezierCurveTo(x-s*.25,y-s*.35,x+sway*.4,y-s*.75,x,y-s*.9);c.bezierCurveTo(x+s*.28,y-s*.45,x+s*.2,y-s*.2,x+s*.18,y);c.closePath();c.fill() }

    function halloween(c,w,h,p) {
        background(c,w,h,"#090611","#351326");moon(c,w*.76,h*.18,Math.min(w,h)*.105,"#ffd8a0")
        c.fillStyle="#5b415c";for(let i=0;i<4;i++){c.globalAlpha=.08+i*.025;const x=((i*.31+p*.008)%1.4-.2)*w,y=h*(.15+i*.07);c.beginPath();c.ellipse(x,y,w*.25,h*.035);c.fill()}c.globalAlpha=1
        c.fillStyle="#0b0910";c.beginPath();c.moveTo(0,h*.79);c.quadraticCurveTo(w*.42,h*.64,w,h*.8);c.lineTo(w,h);c.lineTo(0,h);c.fill()
        const hx=w*.66,hy=h*.53,hs=Math.min(w,h)*.22;c.fillStyle="#100c15";c.fillRect(hx-hs*.45,hy,hs*.9,hs*.72);c.beginPath();c.moveTo(hx-hs*.58,hy);c.lineTo(hx,hy-hs*.48);c.lineTo(hx+hs*.58,hy);c.closePath();c.fill();c.fillRect(hx+hs*.2,hy-hs*.58,hs*.12,hs*.35)
        c.fillStyle="#e68b42";for(let i=-1;i<=1;i+=2){c.globalAlpha=.55+.25*Math.sin(p*2+i);c.fillRect(hx+i*hs*.22-hs*.055,hy+hs*.18,hs*.11,hs*.17)}c.globalAlpha=1
        for(let i=0;i<5;i++)grave(c,w*(.4+i*.1),h*(.84+i%2*.035),Math.min(w,h)*.07,i%2===0)
        pumpkin(c,w*.18,h*.79,Math.min(w,h)*.105);for(let i=0;i<7;i++)bat(c,(i*.157+.06)*w,h*(.12+(i*31%28)/100)+Math.sin(p+i)*4,8+i%3*3,.2+.25*Math.sin(p*3+i))
        c.strokeStyle="#b8adb9";c.globalAlpha=.3;c.lineWidth=.8;for(let i=0;i<=8;i++){const a=i*Math.PI/16;c.beginPath();c.moveTo(0,0);c.lineTo(Math.cos(a)*w*.3,Math.sin(a)*h*.42);c.stroke()}for(let r=1;r<=5;r++){c.beginPath();for(let i=0;i<=8;i++){const a=i*Math.PI/16,x=Math.cos(a)*w*.3*r/5,y=Math.sin(a)*h*.42*r/5;i?c.lineTo(x,y):c.moveTo(x,y)}c.stroke()}c.globalAlpha=1
    }
    function witches(c,w,h,p) {
        background(c,w,h,"#120923","#422044");moon(c,w*.74,h*.23,Math.min(w,h)*.16,"#e8d5ad")
        for(let i=0;i<7;i++)pine(c,w*(.04+i*.16),h*.87,h*(.24+i%3*.05),"#0c1019",false)
        const x=((p*.025)%1.35-.15)*w,y=h*.35+Math.sin(p*.8)*h*.025,s=Math.min(w,h)*.12;c.strokeStyle="#0a0810";c.lineWidth=s*.07;c.beginPath();c.moveTo(x-s*.85,y+s*.45);c.lineTo(x+s*.95,y+s*.18);c.stroke();c.fillStyle="#0a0810";c.beginPath();c.moveTo(x-s*.2,y+s*.15);c.lineTo(x+s*.12,y-s*.55);c.lineTo(x+s*.38,y+s*.12);c.closePath();c.fill();c.fillRect(x-s*.35,y+s*.08,s*.8,s*.1);c.beginPath();c.arc(x,y+s*.26,s*.15,0,Math.PI*2);c.fill();c.beginPath();c.moveTo(x-s*.18,y+s*.4);c.lineTo(x+s*.28,y+s*.35);c.lineTo(x+s*.52,y+s*.85);c.lineTo(x-s*.35,y+s*.72);c.closePath();c.fill()
        const cx=w*.25,cy=h*.82,r=Math.min(w,h)*.12;c.fillStyle="#6ee08c";c.globalAlpha=.12;c.beginPath();c.arc(cx,cy-r*.45,r*1.5,0,Math.PI*2);c.fill();c.globalAlpha=1;c.fillStyle="#0b0d13";c.beginPath();c.ellipse(cx-r,cy-r*.4,r*2,r*.85);c.fill();c.fillRect(cx-r*.82,cy-r*.4,r*1.64,r*.85);c.fillStyle="#71c78a";for(let i=0;i<6;i++){const bx=cx+Math.sin(i*2.1)*r*.65,by=cy-r*.5-((p*.12+i*.17)%1)*r*1.3;c.globalAlpha=.35+i*.08;c.beginPath();c.arc(bx,by,2+i%3,0,Math.PI*2);c.fill()}c.globalAlpha=1
    }
    function monsters(c,w,h,p) {
        background(c,w,h,"#07120e","#1e3625");moon(c,w*.8,h*.16,Math.min(w,h)*.09,"#b9d9a4")
        c.strokeStyle="#101811";c.lineWidth=Math.min(w,h)*.035;c.beginPath();c.moveTo(w*.12,h);c.lineTo(w*.18,h*.45);c.lineTo(w*.08,h*.2);c.moveTo(w*.17,h*.55);c.lineTo(w*.34,h*.3);c.moveTo(w*.16,h*.67);c.lineTo(w*.04,h*.5);c.stroke()
        for(let i=0;i<5;i++){c.fillStyle="#9db8a1";c.globalAlpha=.035+i*.018;const y=h*(.5+i*.09)+Math.sin(p*.4+i)*5;c.beginPath();c.ellipse(w*((i*.23+p*.01)%1),y,w*.38,h*.045);c.fill()}c.globalAlpha=1
        const ex=w*.72,ey=h*.53,er=Math.min(w,h)*.045;c.fillStyle="#d6ee8e";c.globalAlpha=.65+.2*Math.sin(p*1.7);c.beginPath();c.ellipse(ex-er*2.2,ey-er*.55,er*1.35,er*.8);c.ellipse(ex+er*.85,ey-er*.55,er*1.35,er*.8);c.fill();c.fillStyle="#18200d";c.beginPath();c.ellipse(ex-er*1.75,ey-er*.45,er*.3,er*.6);c.ellipse(ex+er*1.45,ey-er*.45,er*.3,er*.6);c.fill();c.globalAlpha=1
        c.fillStyle="#0a100c";c.beginPath();c.moveTo(0,h*.85);c.quadraticCurveTo(w*.5,h*.72,w,h*.86);c.lineTo(w,h);c.lineTo(0,h);c.fill();for(let i=0;i<6;i++)grave(c,w*(.27+i*.1),h*(.87+i%2*.03),Math.min(w,h)*.065,i%2===1)
    }
    function vampires(c,w,h,p) {
        background(c,w,h,"#10050d","#3b0c22");moon(c,w*.72,h*.2,Math.min(w,h)*.12,"#d66565")
        c.fillStyle="#09070e";const base=h*.77,s=Math.min(w,h)*.2;c.fillRect(w*.54,base-s*.55,w*.38,s*.55);for(let i=0;i<4;i++){const x=w*(.54+i*.11);c.fillRect(x,base-s*(.85+i%2*.18),s*.28,s*(.85+i%2*.18));c.beginPath();c.moveTo(x-s*.05,base-s*(.85+i%2*.18));c.lineTo(x+s*.14,base-s*(1.2+i%2*.18));c.lineTo(x+s*.33,base-s*(.85+i%2*.18));c.closePath();c.fill()}
        c.fillStyle="#d14b48";for(let i=0;i<7;i++){c.globalAlpha=.35+.25*Math.sin(p*2+i);c.fillRect(w*(.565+i*.045),base-s*.35,s*.055,s*.13)}c.globalAlpha=1;for(let i=0;i<8;i++)bat(c,w*(.1+i*.105),h*(.17+(i*19%25)/100)+Math.sin(p+i)*5,7+i%3*3,.25+.25*Math.sin(p*2+i))
        c.fillStyle="#08070b";c.fillRect(0,h*.83,w,h*.17);const x=w*.2,y=h*.84;c.strokeStyle="#8b6945";c.lineWidth=3;c.beginPath();c.moveTo(x,y);c.lineTo(x,y-h*.19);c.moveTo(x-w*.06,y-h*.1);c.lineTo(x+w*.06,y-h*.1);c.stroke();for(let i=-1;i<=1;i++){c.fillStyle="#d8c2a0";c.fillRect(x+i*w*.055-2,y-h*(i===0?.23:.15),4,h*.06);flame(c,x+i*w*.055,y-h*(i===0?.23:.15),7,p+i)}
    }
    function christmas(c,w,h,p) {
        background(c,w,h,"#071b25","#16362d");moon(c,w*.18,h*.17,Math.min(w,h)*.085,"#ffe8b5");snowfall(c,w,h,p,44)
        c.fillStyle="#dbe9e5";c.beginPath();c.moveTo(0,h*.83);c.quadraticCurveTo(w*.45,h*.74,w,h*.85);c.lineTo(w,h);c.lineTo(0,h);c.fill();for(let i=0;i<5;i++)pine(c,w*(.04+i*.09),h*.85,h*(.18+i%2*.06),"#12352b",true)
        const x=w*.76,base=h*.86,height=Math.min(w,h)*.52;pine(c,x,base,height,"#0d4a31",false);star(c,x,base-height*1.03,height*.07,"#ffd86a")
        const colors=["#ff4653","#ffd45e","#4fd67c","#62bfff"];for(let i=0;i<18;i++){const yy=base-height*.78+(i%6)*height*.115,xx=x+Math.sin(i*2.3)*height*(.08+(i%6)*.025),color=colors[(i+Math.floor(p*2))%4];c.fillStyle=color;c.globalAlpha=.2;c.beginPath();c.arc(xx,yy,6,0,Math.PI*2);c.fill();c.globalAlpha=.9;c.beginPath();c.arc(xx,yy,2.2,0,Math.PI*2);c.fill()}c.globalAlpha=1
        for(let i=0;i<3;i++){const gx=x-height*.28+i*height*.22,gy=base+5,gw=height*.17;c.fillStyle=i%2?"#b82e42":"#c79735";c.fillRect(gx,gy-gw*.55,gw,gw*.55);c.fillStyle="#e8d6a0";c.fillRect(gx+gw*.43,gy-gw*.55,gw*.14,gw*.55);c.fillRect(gx,gy-gw*.38,gw,gw*.12)}
    }
    function snow(c,w,h,p) {
        background(c,w,h,"#0b1d36","#31506a");moon(c,w*.8,h*.17,Math.min(w,h)*.1,"#e7f1e8")
        c.fillStyle="#b9d0df";c.beginPath();c.moveTo(0,h*.72);c.quadraticCurveTo(w*.4,h*.58,w,h*.76);c.lineTo(w,h);c.lineTo(0,h);c.fill();for(let i=0;i<8;i++)pine(c,w*(.03+i*.135),h*.78,h*(.18+i%3*.04),"#173b3a",true)
        for(let i=0;i<5;i++){const x=w*(.12+i*.17),y=h*(.8+i%2*.035),s=Math.min(w,h)*.1;c.fillStyle=i%2?"#263c50":"#31455a";c.fillRect(x,y-s*.55,s*1.25,s*.55);c.beginPath();c.moveTo(x-s*.1,y-s*.55);c.lineTo(x+s*.62,y-s);c.lineTo(x+s*1.35,y-s*.55);c.closePath();c.fill();c.fillStyle="#ffd88a";c.globalAlpha=.7;c.fillRect(x+s*.25,y-s*.36,s*.22,s*.23);c.fillRect(x+s*.75,y-s*.36,s*.22,s*.23);c.globalAlpha=1}
        const x=w*.25,y=h*.87,r=Math.min(w,h)*.045;c.fillStyle="#e6f1f5";c.beginPath();c.arc(x,y-r,r,0,Math.PI*2);c.arc(x,y-r*2.6,r*.75,0,Math.PI*2);c.arc(x,y-r*3.75,r*.55,0,Math.PI*2);c.fill();c.fillStyle="#1c2830";c.beginPath();c.arc(x-r*.2,y-r*3.9,1.5,0,Math.PI*2);c.arc(x+r*.2,y-r*3.9,1.5,0,Math.PI*2);c.fill();c.fillStyle="#e58a43";c.beginPath();c.moveTo(x,y-r*3.7);c.lineTo(x+r*.75,y-r*3.55);c.lineTo(x,y-r*3.4);c.fill();snowfall(c,w,h,p,64)
    }
    function fireplace(c,w,h,p) {
        background(c,w,h,"#1c1010","#4b2419");c.fillStyle="#152538";c.fillRect(w*.07,h*.12,w*.32,h*.42);c.strokeStyle="#74513a";c.lineWidth=8;c.strokeRect(w*.07,h*.12,w*.32,h*.42);c.lineWidth=3;c.beginPath();c.moveTo(w*.23,h*.12);c.lineTo(w*.23,h*.54);c.moveTo(w*.07,h*.33);c.lineTo(w*.39,h*.33);c.stroke();for(let i=0;i<4;i++)pine(c,w*(.1+i*.08),h*.52,h*.18,"#102b2c",true);snowfall(c,w*.32,h*.42,p,18)
        c.fillStyle="#213426";c.fillRect(w*.48,h*.22,w*.46,h*.07);for(let i=0;i<12;i++){const x=w*(.49+i*.04),color=["#ff4653","#ffd45e","#4fd67c"][i%3];c.fillStyle=color;c.globalAlpha=.65+.25*Math.sin(p*3+i);c.beginPath();c.arc(x,h*.25,2.5,0,Math.PI*2);c.fill()}c.globalAlpha=1
        for(let i=0;i<3;i++){const x=w*(.57+i*.12);c.fillStyle=i%2?"#b12d3d":"#d7c4a1";c.fillRect(x,h*.28,w*.07,h*.15);c.beginPath();c.moveTo(x,h*.4);c.lineTo(x+w*.07,h*.4);c.lineTo(x+w*.085,h*.46);c.lineTo(x+w*.035,h*.47);c.lineTo(x,h*.43);c.closePath();c.fill()}
        const x=w*.58,y=h*.54,fw=w*.34,fh=h*.4;c.fillStyle="#5c3528";c.fillRect(x,y,fw,fh);c.fillStyle="#170d0c";c.fillRect(x+fw*.1,y+fh*.17,fw*.8,fh*.83);c.fillStyle="#7a4a30";c.fillRect(x-fw*.03,y-fh*.05,fw*1.06,fh*.09);c.strokeStyle="#6e3b25";c.lineWidth=7;c.beginPath();c.moveTo(x+fw*.25,y+fh*.86);c.lineTo(x+fw*.7,y+fh*.65);c.moveTo(x+fw*.35,y+fh*.65);c.lineTo(x+fw*.78,y+fh*.86);c.stroke();for(let i=0;i<5;i++)flame(c,x+fw*(.3+i*.1),y+fh*.82,12+i%2*5,p+i)
    }

    onPaint: {
        const c=getContext("2d");c.reset();const w=width,h=height,p=phase
        if(mode==="Halloween"){halloween(c,w,h,p);return}if(mode==="Bruxas"){witches(c,w,h,p);return}if(mode==="Monstros"){monsters(c,w,h,p);return}if(mode==="Vampiros"){vampires(c,w,h,p);return}
        if(mode==="Natal"){christmas(c,w,h,p);return}if(mode==="Neve"){snow(c,w,h,p);return}if(mode==="Lareira"){fireplace(c,w,h,p);return}
        c.strokeStyle="#62dfcf";c.fillStyle="#62dfcf";c.lineWidth=2
        if(mode==="Aurora")for(let i=0;i<3;i++){let x=w*(.5+.35*Math.sin(p*.4+i)),y=h*(.5+.3*Math.cos(p*.3+i)),g=c.createRadialGradient(x,y,0,x,y,Math.max(1,w*.65));g.addColorStop(0,["#185575","#302c5c","#155345"][i]);g.addColorStop(1,"transparent");c.fillStyle=g;c.fillRect(0,0,w,h)}
        if(mode==="Estrelas")for(let i=0;i<40;i++){c.globalAlpha=.35+.3*(1+Math.sin(p+i));c.beginPath();c.arc((i*73%101)*w/101,(i*47%103)*h/103,1+i%3,0,Math.PI*2);c.fill()}
        if(mode==="Chuva")for(let i=0;i<24;i++){let x=(i*73%101)*w/101,y=(i*.087+p*.2)%1*h;c.beginPath();c.moveTo(x,y);c.lineTo(x-w*.018,y+h*.045);c.stroke()}
        if(mode==="Orbitas")for(let i=0;i<4;i++){let r=Math.min(w,h)*(.12+i*.09);c.beginPath();c.arc(w/2,h/2,r,0,Math.PI*2);c.stroke();c.beginPath();c.arc(w/2+Math.cos(p/(i+1)+i)*r,h/2+Math.sin(p/(i+1)+i)*r,4,0,Math.PI*2);c.fill()}
        if(mode==="Pulso")for(let i=0;i<4;i++){let f=(p*.28+i*.25)%1;c.globalAlpha=.7*(1-f);c.beginPath();c.arc(w/2,h/2,Math.min(w,h)*f*.7,0,Math.PI*2);c.stroke()}
        if(mode==="Grade"){c.globalAlpha=.5;c.beginPath();for(let i=-6;i<=6;i++){c.moveTo(w/2+i*w/20,h*.25);c.lineTo(w/2+i*w/4,h)}for(let i=0;i<10;i++){let t=(i+p*.35)%10/10,y=h*(.25+.75*t*t);c.moveTo(0,y);c.lineTo(w,y)}c.stroke()}
        const data=mode==="Ondas"?metrics.waveform:metrics.spectrum
        if((mode==="Ondas"||mode==="Espectro")&&data&&data.length&&playing){c.beginPath();for(let i=0;i<data.length;i++)if(mode==="Ondas"){let y=h*(.5+data[i]*.35);i?c.lineTo(i*w/(data.length-1),y):c.moveTo(0,y)}else c.fillRect(i*w/data.length,h*(1-data[i]*.8),w/data.length-2,h*data[i]*.8);c.stroke()}
    }
}
