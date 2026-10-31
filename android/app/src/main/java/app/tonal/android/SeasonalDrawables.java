package app.tonal.android;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.View;
import android.widget.SeekBar;

final class SeasonalDrawables {
    private static final int[] LIGHTS={0xffff4653,0xffffd45e,0xff4fd67c,0xff62bfff};
    private SeasonalDrawables(){}

    static boolean visible(View host,Rect area){return host.isAttachedToWindow()&&host.isShown()&&host.hasWindowFocus()&&ValueAnimator.areAnimatorsEnabled()&&host.getGlobalVisibleRect(area);}

    static final class TransportGlyph extends Drawable {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Path path=new Path();private ValueAnimator animator;
        boolean playing;float mix;private int alpha=255;
        TransportGlyph(boolean playing,int color){this.playing=playing;mix=playing?1:0;paint.setColor(color);}
        void setColor(int color){paint.setColor(color);invalidateSelf();}
        void setPlaying(boolean value){
            if(playing==value)return;playing=value;float target=value?1:0;
            if(animator!=null){animator.cancel();animator=null;}
            if(!ValueAnimator.areAnimatorsEnabled()){mix=target;invalidateSelf();return;}
            animator=ValueAnimator.ofFloat(mix,target);animator.setDuration(340);animator.setInterpolator(new android.view.animation.AccelerateDecelerateInterpolator());
            animator.addUpdateListener(a->{mix=(float)a.getAnimatedValue();invalidateSelf();});
            animator.addListener(new android.animation.AnimatorListenerAdapter(){public void onAnimationEnd(android.animation.Animator animation){animator=null;}});animator.start();
        }
        public void draw(Canvas canvas){
            canvas.save();Rect b=getBounds();float side=Math.min(b.width(),b.height());canvas.translate(b.left+(b.width()-side)/2f,b.top+(b.height()-side)/2f);canvas.scale(side/24f,side/24f);
            paint.setStyle(Paint.Style.FILL);paint.setAlpha((int)(alpha*(1-mix)));canvas.save();canvas.rotate(mix*75,12,12);canvas.scale(1-mix*.22f,1-mix*.22f,12,12);
            path.rewind();path.moveTo(7,4);path.lineTo(20,12);path.lineTo(7,20);path.close();canvas.drawPath(path,paint);canvas.restore();
            paint.setAlpha((int)(alpha*mix));float spread=4+mix*4,width=2.4f+mix*1.6f,top=4+(1-mix)*4,height=16-(1-mix)*8;
            canvas.drawRoundRect(12-spread-width/2,top,12-spread+width/2,top+height,1,1,paint);canvas.drawRoundRect(12+spread-width/2,top,12+spread+width/2,top+height,1,1,paint);canvas.restore();
        }
        public void setAlpha(int a){alpha=a;invalidateSelf();}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    static final class ChristmasRing extends Drawable implements Runnable {
        final View host;final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);final Rect area=new Rect();float phase;boolean scheduled;
        ChristmasRing(View host){this.host=host;}
        boolean active(){return visible(host,area);}
        public void run(){scheduled=false;if(active()){phase+=.067f;invalidateSelf();}}
        public void draw(Canvas c){
            Rect b=getBounds();float cx=b.exactCenterX(),cy=b.exactCenterY(),r=Math.min(b.width(),b.height())*.46f;
            paint.setStyle(Paint.Style.FILL);paint.setAlpha(255);paint.setColor(0xff0c2118);c.drawCircle(cx,cy,r,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(2,r*.15f));paint.setColor(0xff24583b);c.drawCircle(cx,cy,r*.91f,paint);
            paint.setStrokeWidth(Math.max(1,r*.045f));paint.setColor(0xccb88a35);c.drawCircle(cx,cy,r*.78f,paint);
            paint.setStyle(Paint.Style.FILL);for(int i=0;i<12;i++){double a=i*Math.PI*2/12-Math.PI/2;float x=cx+(float)Math.cos(a)*r*.86f,y=cy+(float)Math.sin(a)*r*.86f,glow=.45f+.4f*(.5f+.5f*(float)Math.sin(phase*3+i*1.7));
                paint.setColor(LIGHTS[(i+(int)phase)%LIGHTS.length]);paint.setAlpha((int)(255*glow*.35f));c.drawCircle(x,y,Math.max(2,r*.13f),paint);paint.setAlpha((int)(255*(.65f+glow*.35f)));c.drawCircle(x,y,Math.max(1.2f,r*.055f),paint);}paint.setAlpha(255);
            if(active()&&!scheduled){scheduled=true;scheduleSelf(this,SystemClock.uptimeMillis()+67);}else if(!active()&&scheduled){scheduled=false;unscheduleSelf(this);}
        }
        public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    static final class Track extends Drawable {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Path path=new Path();private final float density,barHeight;
        Track(Context context,float barHeight){density=context.getResources().getDisplayMetrics().density;this.barHeight=barHeight;}
        public void draw(Canvas c){wire(c,paint,path,getBounds(),density,barHeight,false,0);}
        public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    static final class Trail extends Drawable implements Runnable {
        final SeekBar host;final java.util.function.BooleanSupplier playing;final float density,barHeight;final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);final Path path=new Path();final Rect area=new Rect();float phase;boolean scheduled;
        Trail(SeekBar host,java.util.function.BooleanSupplier playing,float barHeight){this.host=host;this.playing=playing;this.barHeight=barHeight;density=host.getResources().getDisplayMetrics().density;}
        boolean active(){return host.isEnabled()&&host.getProgress()>0&&playing.getAsBoolean()&&visible(host,area);}
        public void run(){scheduled=false;if(active()){phase+=.067f;invalidateSelf();}}
        public void draw(Canvas c){wire(c,paint,path,getBounds(),density,barHeight,true,phase);if(active()&&!scheduled){scheduled=true;scheduleSelf(this,SystemClock.uptimeMillis()+67);}else if(!active()&&scheduled){scheduled=false;unscheduleSelf(this);}}
        public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    private static void wire(Canvas c,Paint paint,Path path,Rect b,float density,float barHeight,boolean lit,float phase){
        float cy=b.exactCenterY()-2*density,w=b.width();paint.setStyle(Paint.Style.STROKE);paint.setAlpha(255);paint.setStrokeWidth(Math.max(2,barHeight*.42f));paint.setColor(lit?0xff467a4e:0xff345c43);path.rewind();path.moveTo(b.left,cy);
        for(float x=0;x<=w;x+=8*density)path.lineTo(b.left+x,cy+(float)Math.sin(x/density*.16f)*1.2f*density);c.drawPath(path,paint);
        paint.setStrokeWidth(density);paint.setColor(0xff172e22);c.drawLine(b.left,cy-density,b.right,cy-density,paint);paint.setStyle(Paint.Style.FILL);
        int count=Math.max(5,(int)(w/(42*density)));float size=Math.max(4*density,Ui.THUMB_SIZE*density*.25f);
        for(int i=0;i<=count;i++){float x=b.left+w*i/count,color=lit?LIGHTS[(i+(int)(phase*.9f))%LIGHTS.length]:0xff63736a,glow=.5f+.5f*(float)Math.sin(phase*4+i);
            if(lit){paint.setColor((int)color);paint.setAlpha((int)(255*(.12f+glow*.15f)));c.drawCircle(x,cy+size*.45f,size*1.65f,paint);paint.setAlpha(255);}
            paint.setColor(0xff263d32);c.drawRect(x-size*.27f,cy+2*density-size*.35f,x+size*.27f,cy+2*density+size*.07f,paint);paint.setColor((int)color);c.drawOval(x-size*.48f,cy+2*density,x+size*.48f,cy+2*density+size*1.25f,paint);
            if(lit){paint.setColor(0x8cfff7d6);c.drawOval(x-size*.22f,cy+2*density+size*.16f,x-size*.05f,cy+2*density+size*.52f,paint);}
        }paint.setAlpha(255);
    }

    static final class Star extends Drawable {
        private final int size;private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Path path=new Path();private Shader gradient;
        Star(int size){this.size=size;}
        protected void onBoundsChange(Rect b){gradient=new LinearGradient(b.left,b.top,b.right,b.bottom,0xfffff7bd,0xffc78618,Shader.TileMode.CLAMP);}
        public void draw(Canvas c){Rect b=getBounds();float cx=b.exactCenterX(),cy=b.exactCenterY(),outer=size*.48f,inner=outer*.44f;
            paint.setStyle(Paint.Style.FILL);paint.setColor(0x38ffe795);c.drawCircle(cx,cy,size*.62f,paint);path.rewind();for(int i=0;i<10;i++){double a=-Math.PI/2+i*Math.PI/5;float r=i%2==1?inner:outer,x=cx+(float)Math.cos(a)*r,y=cy+(float)Math.sin(a)*r;if(i==0)path.moveTo(x,y);else path.lineTo(x,y);}path.close();paint.setShader(gradient);c.drawPath(path,paint);paint.setShader(null);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(1,size*.045f));paint.setColor(0xfffff4c2);c.drawPath(path,paint);
        }
        public int getIntrinsicWidth(){return size;}public int getIntrinsicHeight(){return size;}public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    static final class Cover extends Drawable implements Runnable {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Path path=new Path();private final Rect area=new Rect();private final float density;private Shader spiderGradient;float phase;private boolean scheduled;
        Cover(Context context){density=context.getResources().getDisplayMetrics().density;}
        protected void onBoundsChange(Rect b){float size=Math.max(5,Math.min(13,Math.min(b.width(),b.height())*.035f));spiderGradient=new RadialGradient(-size*.25f,-size*.4f,size*1.2f,new int[]{0xff5e555e,0xff282129,0xff08070a},new float[]{0,.35f,1},Shader.TileMode.CLAMP);}
        private View host(){return getCallback() instanceof View?(View)getCallback():null;}
        private boolean active(){View v=host();Rect b=getBounds();return v!=null&&Math.min(b.width(),b.height())>=120*density&&visible(v,area);}
        public void run(){scheduled=false;if(active()){phase+=.067f;invalidateSelf();}}
        public void draw(Canvas c){Rect b=getBounds();if(Ui.CHRISTMAS)drawChristmas(c,b);else if(Ui.HALLOWEEN)drawHalloween(c,b);if(active()&&!scheduled){scheduled=true;scheduleSelf(this,SystemClock.uptimeMillis()+67);}else if(!active()&&scheduled){scheduled=false;unscheduleSelf(this);}}
        private void drawChristmas(Canvas c,Rect b){
            float w=b.width(),m=Math.min(w,b.height());paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2.2f*density);paint.setColor(0xff1c3525);path.rewind();path.moveTo(b.left,b.top+8*density);path.quadTo(b.left+w*.5f,b.top+44*density,b.right,b.top+8*density);c.drawPath(path,paint);paint.setStyle(Paint.Style.FILL);
            int count=Math.max(7,(int)(w/(42*density)));for(int i=0;i<=count;i++){float t=(float)i/count,x=b.left+t*w,y=b.top+8*density+56*t*(1-t)*density,size=Math.max(3*density,m*.018f),glow=.5f+.5f*(float)Math.sin(phase*3+i);paint.setColor(LIGHTS[(i+(int)(phase*.8f))%LIGHTS.length]);paint.setAlpha((int)(255*(.10f+glow*.14f)));c.drawCircle(x,y+5*density,size*2.1f,paint);paint.setAlpha((int)(255*(.8f+glow*.2f)));c.drawOval(x-size*.55f,y+5*density-size*.15f,x+size*.55f,y+5*density+size*1.4f,paint);paint.setColor(0xa6fff8dc);c.drawOval(x-size*.25f,y+5*density,x-size*.07f,y+5*density+size*.5f,paint);}paint.setAlpha(255);
        }
        private void web(Canvas c,float x,float y,float radius,float start,float end,int spokes,int rings){
            c.save();c.translate(x,y);paint.setStyle(Paint.Style.STROKE);for(int pass=0;pass<2;pass++){paint.setColor(pass==1?0x94d9d5d2:0xb327232b);paint.setStrokeWidth((pass==1?.65f:1.8f)*density);
                for(int i=0;i<=spokes;i++){double a=start+(end-start)*i/spokes;path.rewind();path.moveTo(0,0);path.quadTo((float)Math.cos(a+.018)*radius*.52f,(float)Math.sin(a+.018)*radius*.52f,(float)Math.cos(a)*radius,(float)Math.sin(a)*radius);c.drawPath(path,paint);}
                for(int r=1;r<=rings;r++){float fraction=(float)r/rings;path.rewind();for(int i=0;i<=spokes;i++){double a=start+(end-start)*i/spokes;float x1=(float)Math.cos(a)*radius*fraction,y1=(float)Math.sin(a)*radius*fraction;if(i==0)path.moveTo(x1,y1);else{double mid=a-(end-start)/spokes*.5;path.quadTo((float)Math.cos(mid)*radius*fraction*.91f,(float)Math.sin(mid)*radius*fraction*.91f,x1,y1);}}c.drawPath(path,paint);}
            }c.restore();
        }
        private void spider(Canvas c,float x,float y,float size,float weave){
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(.7f*density);paint.setColor(0x8cd7d1cf);c.drawLine(x,0,x,y-size*.6f,paint);
            if(weave>0){paint.setColor(0x8cc8c1c2);path.rewind();for(int i=0;i<28;i++){double a=i*.65;float r=weave*i/28,px=x+(float)Math.cos(a)*r,py=y+size+(float)Math.sin(a)*r;if(i==0)path.moveTo(px,py);else path.lineTo(px,py);}c.drawPath(path,paint);}
            c.save();c.translate(x,y);paint.setColor(0xff17131a);paint.setStrokeWidth(Math.max(density,size*.14f));paint.setStrokeCap(Paint.Cap.ROUND);
            for(int side=-1;side<=1;side+=2)for(int i=0;i<4;i++){float yy=(i-1.5f)*size*.34f,reach=size*(1.15f+(i==1||i==2?.3f:0));path.rewind();path.moveTo(side*size*.35f,yy);path.lineTo(side*size*.9f,yy+(i-1.5f)*size*.25f);path.lineTo(side*reach,yy+(i-1.5f)*size*.65f);c.drawPath(path,paint);}
            paint.setStyle(Paint.Style.FILL);paint.setShader(spiderGradient);c.drawOval(-size*.65f,-size*.8f,size*.65f,size*.65f,paint);c.drawOval(-size*.43f,size*.25f,size*.43f,size*1.1f,paint);paint.setShader(null);paint.setColor(0xbfa52b35);path.rewind();path.moveTo(0,-size*.6f);path.lineTo(size*.28f,-size*.15f);path.lineTo(0,size*.05f);path.lineTo(-size*.28f,-size*.15f);path.close();c.drawPath(path,paint);paint.setColor(0xffd9c9b7);for(int side=-1;side<=1;side+=2)c.drawCircle(side*size*.2f,size*.58f,Math.max(.6f,size*.07f),paint);c.restore();
        }
        private void drawHalloween(Canvas c,Rect b){
            float w=b.width(),h=b.height(),m=Math.min(w,h);web(c,b.left,b.top,m*.62f,0,(float)Math.PI/2,9,7);web(c,b.right,b.top,m*.31f,(float)Math.PI/2,(float)Math.PI,6,4);
            float cycle=phase%14,y=h*.18f,weave=0;if(cycle>=2&&cycle<6)y=h*(.18f+(cycle-2)/4*.52f);else if(cycle>=6&&cycle<8){y=h*.70f;weave=(cycle-6)*m*.035f;}else if(cycle>=8&&cycle<12)y=h*(.70f-(cycle-8)/4*.52f);
            c.save();c.translate(b.left,b.top);spider(c,w*.76f,y,Math.max(5,Math.min(13,m*.035f)),weave);c.restore();
        }
        public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
}
