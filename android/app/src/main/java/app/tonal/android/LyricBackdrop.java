package app.tonal.android;

import android.content.Context;
import android.graphics.*;
import android.view.View;
import java.util.function.*;

/** Fixed primitive budgets and reusable draw objects. Scheduling belongs to the visible stage. */
class LyricBackdrop extends View {
    private final Supplier<String> mode;private final BooleanSupplier active;
    private final Paint paint=new Paint(3);private final Path path=new Path();private final Matrix matrix=new Matrix();
    private final RadialGradient[] gradients=new RadialGradient[3];private float radius;
    LyricBackdrop(Context c,Supplier<String> mode,BooleanSupplier active){super(c);this.mode=mode;this.active=active;}
    static boolean animated(String mode){return !mode.equals("Capa")&&!mode.equals("Foto")&&!mode.equals("Sem animação");}
    protected void onSizeChanged(int w,int h,int oldw,int oldh){radius=Math.max(1,w*.65f);int[] colors={0xff185575,0xff302c5c,0xff155345};for(int i=0;i<3;i++)gradients[i]=new RadialGradient(0,0,radius,colors[i],Color.TRANSPARENT,Shader.TileMode.CLAMP);}
    protected void onDraw(Canvas c){float w=getWidth(),h=getHeight();if(w<=0||h<=0)return;double phase=active.getAsBoolean()?android.os.SystemClock.elapsedRealtime()/2200.:0;String m=mode.get();paint.setColor(0xff62dfcf);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2*getResources().getDisplayMetrics().density);
        switch(m){
            case "Aurora":for(int i=0;i<3;i++){matrix.setTranslate((float)(w*(.5+.35*Math.sin(phase*.4+i))),(float)(h*(.5+.3*Math.cos(phase*.3+i))));if(gradients[i]==null)continue;gradients[i].setLocalMatrix(matrix);paint.setShader(gradients[i]);c.drawRect(0,0,w,h,paint);}paint.setShader(null);break;
            case "Estrelas":paint.setStyle(Paint.Style.FILL);for(int i=0;i<40;i++){paint.setAlpha(80+(int)(70*(1+Math.sin(phase+i))));c.drawCircle(((i*73)%101)*w/101,((i*47)%103)*h/103,1+i%3,paint);}paint.setAlpha(255);break;
            case "Chuva":for(int i=0;i<24;i++){float x=((i*73)%101)*w/101,y=(float)((i*.087+phase*.20)%1)*h;c.drawLine(x,y,x-w*.018f,y+h*.045f,paint);}break;
            case "Orbitas":for(int i=0;i<4;i++){float r=Math.min(w,h)*(.12f+i*.09f);c.drawCircle(w/2,h/2,r,paint);c.drawCircle(w/2+(float)Math.cos(phase/(i+1)+i)*r,h/2+(float)Math.sin(phase/(i+1)+i)*r,4,paint);}break;
            case "Pulso":for(int i=0;i<4;i++){float fraction=(float)((phase*.28+i*.25)%1);paint.setAlpha((int)(180*(1-fraction)));c.drawCircle(w/2,h/2,Math.min(w,h)*fraction*.7f,paint);}paint.setAlpha(255);break;
            case "Grade":paint.setAlpha(100);for(int i=-6;i<=6;i++)c.drawLine(w/2+i*w/20,h*.25f,w/2+i*w/4,h,paint);for(int i=0;i<10;i++){float t=(float)((i+phase*.35)%10)/10;float y=h*(.25f+.75f*t*t);c.drawLine(0,y,w,y,paint);}paint.setAlpha(255);break;
            case "Ondas":case "Espectro":float[] data=m.equals("Ondas")?NativeBridge.audio.waveform:NativeBridge.audio.spectrum;if(data.length<2)break;if(m.equals("Ondas")){path.rewind();for(int i=0;i<data.length;i++){float x=i*w/(data.length-1),y=h*(.5f+data[i]*.35f);if(i==0)path.moveTo(x,y);else path.lineTo(x,y);}c.drawPath(path,paint);}else{paint.setStyle(Paint.Style.FILL);for(int i=0;i<data.length;i++)c.drawRect(i*w/data.length,h*(1-data[i]*.8f),(i+1)*w/data.length-2,h,paint);}break;
        }
    }
}
