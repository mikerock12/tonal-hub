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
        boolean active(){return (NativeBridge.playing||IndiePlaybackService.playing)&&visible(host,area)&&ChristmasLights.allowed(host.getContext());}
        public void run(){if(active()){phase+=ChristmasLights.STEP;invalidateSelf();}else{scheduled=false;ChristmasLights.leave(this);}}
        public void draw(Canvas c){
            Rect b=getBounds();float cx=b.exactCenterX(),cy=b.exactCenterY(),r=Math.min(b.width(),b.height())*.46f;
            paint.setStyle(Paint.Style.FILL);paint.setAlpha(255);paint.setColor(0xff0c2118);c.drawCircle(cx,cy,r,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(2,r*.15f));paint.setColor(0xff24583b);c.drawCircle(cx,cy,r*.91f,paint);
            paint.setStrokeWidth(Math.max(1,r*.045f));paint.setColor(0xccb88a35);c.drawCircle(cx,cy,r*.78f,paint);
            paint.setStyle(Paint.Style.FILL);for(int i=0;i<12;i++){double a=i*Math.PI*2/12-Math.PI/2;float x=cx+(float)Math.cos(a)*r*.86f,y=cy+(float)Math.sin(a)*r*.86f,glow=.45f+.4f*(.5f+.5f*(float)Math.sin(phase*3+i*1.7));
                paint.setColor(LIGHTS[(i+(int)phase)%LIGHTS.length]);paint.setAlpha((int)(255*glow*.35f));c.drawCircle(x,y,Math.max(2,r*.13f),paint);paint.setAlpha((int)(255*(.65f+glow*.35f)));c.drawCircle(x,y,Math.max(1.2f,r*.055f),paint);}paint.setAlpha(255);
            if(!scheduled&&active()){scheduled=true;ChristmasLights.join(this);}
        }
        public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    private static final float SEEK_SPACING_DP=28;
    private static int bulbCount(float width,float density){return Math.max(6,(int)(width/(SEEK_SPACING_DP*density)));}
    private static float bulbLength(float density){return Math.max(14,Math.min(19,Ui.THUMB_SIZE*.8f))*density;}
    private static float bulbAngle(int i){return (i%3==2?0:180)+(float)Math.sin(i*2.4)*12;}

    /** Wire with every bulb dark; the lit ones are painted over it by Trail. */
    static final class Track extends Drawable {
        private final Context context;private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);private final RectF tile=new RectF();private final float density;private ChristmasAssets art;private Bitmap cached;
        Track(Context context,float barHeight){this.context=context;density=context.getResources().getDisplayMetrics().density;art=ChristmasAssets.get(context,()->{art=ChristmasAssets.get(context,null);invalidateSelf();});}
        /** The dark string never changes: painted once, then a single bitmap per frame. */
        private Bitmap render(int w,int h){
            Bitmap bitmap=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(bitmap);float cy=h/2f,height=16*density,width=art.wire.getWidth()*height/art.wire.getHeight();
            for(float x=0;x<w;x+=width){tile.set(x,cy-height/2,x+width,cy+height/2);c.drawBitmap(art.wire,null,tile,paint);}
            int count=bulbCount(w,density);float length=bulbLength(density);
            for(int i=0;i<count;i++){
                c.save();c.translate(w*(i+.5f)/count,cy);c.rotate(bulbAngle(i));
                float sw=length*.46f,sh=length*.52f;tile.set(-sw/2,-sh*.92f,sw/2,sh*.08f);c.drawBitmap(art.socket,null,tile,paint);
                float gy=-length*1.28f,side=length*3.5f;tile.set(-side/2,gy-side/2,side/2,gy+side/2);c.drawBitmap(art.off[i%4],null,tile,paint);c.restore();
            }
            return bitmap;
        }
        public void draw(Canvas c){
            Rect b=getBounds();if(art==null||b.isEmpty())return;
            if(cached==null||cached.getWidth()!=b.width()||cached.getHeight()!=b.height()){if(cached!=null)cached.recycle();cached=render(b.width(),b.height());}
            c.drawBitmap(cached,b.left,b.top,null);
        }
        public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    /** Lit bulbs. A bulb stays dark until the star reaches it, flares, then twinkles. */
    static final class Trail extends Drawable implements Runnable {
        final SeekBar host;final java.util.function.BooleanSupplier playing;final float density;final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);final RectF tile=new RectF();final Rect area=new Rect();float phase;boolean scheduled;private float[] level=new float[0],flare=new float[0];private ChristmasAssets art;private boolean settling;
        Star star;
        Trail(SeekBar host,java.util.function.BooleanSupplier playing,float barHeight){this.host=host;this.playing=playing;density=host.getResources().getDisplayMetrics().density;art=ChristmasAssets.get(host.getContext(),()->{art=ChristmasAssets.get(host.getContext(),null);invalidateSelf();});}
        float fraction(){return getLevel()/10000f;}
        boolean active(){return host.isEnabled()&&playing.getAsBoolean()&&visible(host,area)&&ChristmasLights.allowed(host.getContext());}
        /** How many bulbs the star has passed; the position alone decides, never the clock. */
        int litBulbs(int count){int lit=0;float edge=fraction();for(int i=0;i<count;i++)if((i+.5f)/count<=edge)lit++;return lit;}
        private boolean settle(int count){
            if(level.length!=count){level=new float[count];flare=new float[count];float edge=fraction();for(int i=0;i<count;i++)level[i]=(i+.5f)/count<=edge?1:0;}
            boolean moving=false;float edge=fraction();
            for(int i=0;i<count;i++){
                float wanted=(i+.5f)/count<=edge?1:0;
                if(wanted==1&&level[i]==0)flare[i]=1;
                if(level[i]!=wanted){level[i]=wanted>level[i]?Math.min(1,level[i]+.27f):Math.max(0,level[i]-.27f);moving=true;}
                if(flare[i]>0){flare[i]=Math.max(0,flare[i]-.09f);moving=true;}
            }
            return moving;
        }
        public void run(){boolean running=active();if(running){phase+=ChristmasLights.STEP;if(star!=null)star.phase=phase;}if(running||settling)invalidateSelf();else{scheduled=false;ChristmasLights.leave(this);}}
        @Override protected boolean onLevelChange(int level){invalidateSelf();return true;}
        public void draw(Canvas c){
            if(art==null)return;Rect b=getBounds();float cy=b.exactCenterY(),w=b.width(),length=bulbLength(density),side=length*3.5f,gy=-length*1.28f;int count=bulbCount(w,density);boolean moving=settle(count);
            for(int i=0;i<count;i++){
                if(level[i]<=.01f)continue;float grow=1+flare[i]*.42f,half=side*grow/2;
                paint.setAlpha((int)(255*Math.min(1,level[i]*(ChristmasLights.twinkle(i,phase)+flare[i]))));
                c.save();c.translate(b.left+w*(i+.5f)/count,cy);c.rotate(bulbAngle(i));tile.set(-half,gy-half,half,gy+half);c.drawBitmap(art.on[i%4],null,tile,paint);c.restore();
            }
            settling=moving;if(!scheduled&&(moving||active())){scheduled=true;ChristmasLights.join(this);}
        }
        public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    /** Thumb: faceted star over a slowly turning glow; both may spill past its bounds. */
    static final class Star extends Drawable {
        private final int size;private final Context context;private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);private final RectF tile=new RectF();private ChristmasAssets art;float phase;
        Star(int size,Context context){this.size=size;this.context=context;art=ChristmasAssets.get(context,()->{art=ChristmasAssets.get(context,null);invalidateSelf();});}
        public void draw(Canvas c){
            if(art==null)return;Rect b=getBounds();float cx=b.exactCenterX(),cy=b.exactCenterY(),pulse=.5f+.5f*(float)Math.sin(phase*2.7f),glow=size*(3.1f+.42f*pulse),body=size*1.9f;
            c.save();c.rotate(phase*9,cx,cy);paint.setAlpha((int)(255*(.62f+.38f*pulse)));tile.set(cx-glow,cy-glow,cx+glow,cy+glow);c.drawBitmap(art.glow,null,tile,paint);c.restore();
            paint.setAlpha(255);tile.set(cx-body,cy-body,cx+body,cy+body);c.drawBitmap(art.star,null,tile,paint);
        }
        public int getIntrinsicWidth(){return size;}public int getIntrinsicHeight(){return size;}public void setAlpha(int a){paint.setAlpha(a);}public void setColorFilter(ColorFilter f){paint.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }

    static final class Cover extends Drawable implements Runnable {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final Path path=new Path();private final Rect area=new Rect();private final float density;private Shader spiderGradient;float phase;private boolean scheduled;private ChristmasAssets art;
        Cover(Context context){density=context.getResources().getDisplayMetrics().density;}
        protected void onBoundsChange(Rect b){float size=Math.max(5,Math.min(13,Math.min(b.width(),b.height())*.035f));spiderGradient=new RadialGradient(-size*.25f,-size*.4f,size*1.2f,new int[]{0xff5e555e,0xff282129,0xff08070a},new float[]{0,.35f,1},Shader.TileMode.CLAMP);}
        private View host(){return getCallback() instanceof View?(View)getCallback():null;}
        private boolean active(){View v=host();Rect b=getBounds();return Ui.HALLOWEEN&&v!=null&&Math.min(b.width(),b.height())>=120*density&&visible(v,area);}
        public void run(){scheduled=false;if(active()){phase+=.067f;invalidateSelf();}}
        public void draw(Canvas c){Rect b=getBounds();if(Ui.CHRISTMAS)drawChristmas(c,b);else if(Ui.HALLOWEEN)drawHalloween(c,b);if(active()&&!scheduled){scheduled=true;scheduleSelf(this,SystemClock.uptimeMillis()+67);}else if(!active()&&scheduled){scheduled=false;unscheduleSelf(this);}}
        /** Ordinary covers: one still image, so a grid of them runs no clock at all. */
        private void drawChristmas(Canvas c,Rect b){
            if(Math.min(b.width(),b.height())<120*density)return;View v=host();if(v==null)return;
            if(art==null)art=ChristmasAssets.get(v.getContext(),this::invalidateSelf);if(art==null)art=ChristmasAssets.get(v.getContext(),null);if(art==null)return;
            paint.setAlpha(255);paint.setFilterBitmap(true);c.drawBitmap(art.lit,null,b,paint);
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
