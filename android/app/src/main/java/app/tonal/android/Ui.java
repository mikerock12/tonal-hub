package app.tonal.android;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.*;
import android.text.TextUtils;
import android.view.*;
import android.widget.*;

/** Small native view vocabulary shared by the catalog and transport. */
final class Ui {
    // Active theme colors (AppTheme.apply). Defaults are the original Tonal Azul.
    static int BG=0xff101112,CARD=0xff202224,PANEL=0xff18191b,TEXT=0xfff5f5f5,MUTED=0xffa5a8ad,BLUE=0xff009bd6,ACCENT_SOFT=0xff7ddbff,TINT=0xff263748,ON_ACCENT=0xff000000;
    // Progress bar and thumb of the active theme.
    static int SEEK_TRACK=0xff404040,SEEK_START=0xff009bd6,SEEK_END=0xff009bd6,THUMB=0xfff5f5f5,THUMB_BORDER=0xfff5f5f5,SEEK_HEIGHT=4,THUMB_SIZE=14;
    static boolean HALLOWEEN,CHRISTMAS;
    static String THUMB_SHAPE="circle";static boolean THUMB_GLOW;
    final Context context;
    Ui(Context context){this.context=context;}
    int dp(int n){return Math.round(n*context.getResources().getDisplayMetrics().density);}
    LinearLayout column(){LinearLayout v=new LinearLayout(context);v.setOrientation(LinearLayout.VERTICAL);return v;}
    LinearLayout row(){LinearLayout v=new LinearLayout(context);v.setGravity(Gravity.CENTER_VERTICAL);return v;}
    TextView text(String s,int size,int color){TextView v=new TextView(context);v.setText(s);v.setTextSize(size);v.setTextColor(color);v.setGravity(Gravity.CENTER_VERTICAL);return v;}
    TextView title(String s,int size){TextView v=text(s,size,TEXT);v.setTypeface(null,Typeface.BOLD);return v;}
    TextView line(String s,int size,int color){TextView v=text(s,size,color);v.setSingleLine();v.setEllipsize(TextUtils.TruncateAt.END);return v;}
    void pad(View v,int x,int y){v.setPadding(dp(x),dp(y),dp(x),dp(y));}
    static void insets(View v){int l=v.getPaddingLeft(),t=v.getPaddingTop(),r=v.getPaddingRight(),b=v.getPaddingBottom();v.setOnApplyWindowInsetsListener((view,insets)->{if(android.os.Build.VERSION.SDK_INT>=30){android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout()|WindowInsets.Type.ime());view.setPadding(l+bars.left,t+bars.top,r+bars.right,b+bars.bottom);}else view.setPadding(l+insets.getSystemWindowInsetLeft(),t+insets.getSystemWindowInsetTop(),r+insets.getSystemWindowInsetRight(),b+insets.getSystemWindowInsetBottom());return insets;});}
    void space(LinearLayout parent,int height){parent.addView(new View(context),new LinearLayout.LayoutParams(1,dp(height)));}
    void weight(LinearLayout parent,View v){parent.addView(v,new LinearLayout.LayoutParams(0,-2,1));}
    GradientDrawable background(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    void rounded(View v,int color,int radius){v.setBackground(background(color,radius));v.setClipToOutline(true);}
    void clickable(View v,Runnable click){v.setOnClickListener(w->click.run());v.setFocusable(true);v.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf(0x335fffff),v.getBackground(),background(0xffffffff,8)));}
    TextView pill(String text,boolean active,Runnable click){TextView v=text(text,13,active?BG:TEXT);v.setGravity(Gravity.CENTER);pad(v,16,11);rounded(v,active?TEXT:CARD,24);clickable(v,click);return v;}
    TextView action(String text,Runnable click){TextView v=text(text,14,TEXT);v.setGravity(Gravity.CENTER);pad(v,16,13);rounded(v,CARD,8);clickable(v,click);return v;}
    boolean seasonal(){return HALLOWEEN||CHRISTMAS;}
    ImageButton icon(String name,String description,Runnable click){ImageButton b=new ImageButton(context);boolean transport=seasonal()&&(name.equals("play")||name.equals("pause"));b.setImageDrawable(transport?new SeasonalDrawables.TransportGlyph(name.equals("pause"),CHRISTMAS?0xffffe39c:BLUE):new Symbol(name,TEXT));b.setScaleType(ImageView.ScaleType.CENTER_INSIDE);b.setContentDescription(description);b.setTooltipText(description);b.setBackground(background(0x00000000,24));b.setPadding(dp(12),dp(12),dp(12),dp(12));b.setMinimumWidth(dp(48));b.setMinimumHeight(dp(48));clickable(b,()->{if(transport){b.animate().cancel();b.setScaleX(.88f);b.setScaleY(.88f);b.animate().scaleX(1f).scaleY(1f).setDuration(220).start();}click.run();});return b;}
    void setTransport(ImageButton button,boolean playing){if(seasonal()){if(button.getDrawable() instanceof SeasonalDrawables.TransportGlyph)((SeasonalDrawables.TransportGlyph)button.getDrawable()).setPlaying(playing);else button.setImageDrawable(new SeasonalDrawables.TransportGlyph(playing,CHRISTMAS?0xffffe39c:BLUE));}else button.setImageDrawable(new Symbol(playing?"pause":"play",TEXT));}
    /** Themed track, gradient fill and thumb shape; the touch target stays 40 dp tall. */
    void seek(SeekBar bar){seek(bar,()->false);}
    void seek(SeekBar bar,java.util.function.BooleanSupplier playing){
        int height=dp(SEEK_HEIGHT);
        GradientDrawable track=new GradientDrawable();track.setColor(SEEK_TRACK);track.setCornerRadius(height);
        GradientDrawable fill=new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,new int[]{SEEK_START,SEEK_END});fill.setCornerRadius(height);
        Drawable trackDrawable=THUMB_SHAPE.equals("star")?new SeasonalDrawables.Track(context,height):track;
        Drawable fillDrawable=THUMB_SHAPE.equals("drop")?new BloodTrail(bar,playing,height):THUMB_SHAPE.equals("star")?new SeasonalDrawables.Trail(bar,playing,height):fill;
        LayerDrawable layers=new LayerDrawable(new Drawable[]{trackDrawable,new ClipDrawable(fillDrawable,Gravity.START,ClipDrawable.HORIZONTAL)});
        layers.setId(0,android.R.id.background);layers.setId(1,android.R.id.progress);
        for(int i=0;i<2;i++){layers.setLayerHeight(i,height);layers.setLayerGravity(i,Gravity.CENTER_VERTICAL|Gravity.FILL_HORIZONTAL);}
        if(THUMB_SHAPE.equals("drop")){layers.setLayerHeight(1,height+dp(32));bar.setMinimumHeight(dp(48));}
        if(THUMB_SHAPE.equals("star")){for(int i=0;i<2;i++)layers.setLayerHeight(i,height+dp(28));bar.setMinimumHeight(dp(48));}
        bar.setProgressDrawable(layers);bar.setThumb(thumb());bar.setSplitTrack(false);bar.setBackground(null);
    }
    Drawable thumb(){
        if(THUMB_SHAPE.equals("drop"))return new BloodDrop(dp(THUMB_SIZE),THUMB,THUMB_BORDER);
        if(THUMB_SHAPE.equals("star"))return new SeasonalDrawables.Star(dp(THUMB_SIZE));
        int size=dp(THUMB_SIZE);boolean pill=THUMB_SHAPE.equals("pill");
        GradientDrawable core=new GradientDrawable();core.setColor(THUMB);
        if(THUMB_SHAPE.equals("ring"))core.setStroke(dp(3),THUMB_BORDER);else if(THUMB_BORDER!=THUMB)core.setStroke(dp(2),THUMB_BORDER);
        if(THUMB_SHAPE.equals("diamond")){core.setCornerRadius(dp(2));int side=Math.round(size*0.75f);core.setSize(side,side);
            RotateDrawable turned=new RotateDrawable();turned.setDrawable(core);turned.setFromDegrees(45);turned.setToDegrees(45);turned.setLevel(1);
            LayerDrawable box=new LayerDrawable(new Drawable[]{turned});box.setLayerSize(0,size,size);return box;}
        core.setCornerRadius(size);core.setSize(pill?Math.round(size*1.6f):size,pill?Math.round(size*0.8f):size);
        if(!THUMB_GLOW)return core;
        GradientDrawable halo=new GradientDrawable();halo.setShape(GradientDrawable.OVAL);halo.setColor((THUMB_BORDER&0x00ffffff)|0x48000000);
        LayerDrawable glow=new LayerDrawable(new Drawable[]{halo,core});int outer=size+dp(10);glow.setLayerSize(0,outer,outer);glow.setLayerSize(1,size,size);glow.setLayerGravity(1,Gravity.CENTER);return glow;
    }
    /** Animated seasonal ring for the primary player controls, confined to the visible view. */
    void halloweenTransport(ImageButton button){if(HALLOWEEN)button.setBackground(new HalloweenTransport(button));else if(CHRISTMAS)button.setBackground(new SeasonalDrawables.ChristmasRing(button));}
    static final class HalloweenTransport extends Drawable implements Runnable {
        final View host;final Paint paint=new Paint(3);final Path path=new Path();final Rect visible=new Rect();
        float phase;boolean scheduled;
        HalloweenTransport(View host){this.host=host;}
        boolean active(){return host.isAttachedToWindow()&&host.isShown()&&host.hasWindowFocus()&&android.animation.ValueAnimator.areAnimatorsEnabled()&&host.getGlobalVisibleRect(visible);}
        public void run(){scheduled=false;if(active()){phase+=.16f;invalidateSelf();}}
        public void draw(Canvas c){Rect b=getBounds();float cx=b.exactCenterX(),cy=b.exactCenterY(),r=Math.min(b.width(),b.height())*.46f;
            paint.setStyle(Paint.Style.FILL);paint.setColor(0xff130d15);c.drawCircle(cx,cy,r,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(1,r*.10f));paint.setColor(0xff3e2933);c.drawCircle(cx,cy,r*.96f,paint);
            float pulse=.5f+.5f*(float)Math.sin(phase);paint.setStrokeWidth(Math.max(1,r*.045f));paint.setColor(0xffff873e);paint.setAlpha((int)(155+100*pulse));c.drawCircle(cx,cy,r*.79f,paint);paint.setAlpha(255);
            // Small flickering embers and blood trails, with fixed bounded geometry.
            paint.setStyle(Paint.Style.FILL);for(int i=0;i<8;i++){double angle=(i*2*Math.PI/8)+.12;float x=cx+(float)Math.cos(angle)*r*.84f,y=cy+(float)Math.sin(angle)*r*.84f;paint.setColor((i%2==0)?0xfffa7135:0xffb22b25);paint.setAlpha((int)(100+120*(.5+.5*Math.sin(phase+i))));c.drawCircle(x,y,Math.max(1,r*.035f),paint);}paint.setAlpha(255);
            paint.setColor(0xff9f1d23);for(int i=-1;i<=1;i++){float x=cx+i*r*.35f,y=cy+r*.76f,len=r*(.12f+.08f*(float)(.5+.5*Math.sin(phase+i)));c.drawRoundRect(x-r*.025f,y,x+r*.025f,y+len,r*.025f,r*.025f,paint);}
            if(active()&&!scheduled){scheduled=true;scheduleSelf(this,android.os.SystemClock.uptimeMillis()+67);}else if(!active()&&scheduled){scheduled=false;unscheduleSelf(this);}
        }
        public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
    static final class BloodTrail extends Drawable implements Runnable {
        final SeekBar host;final java.util.function.BooleanSupplier playing;final float barHeight,density;
        final Paint paint=new Paint(3);final Path path=new Path();final Rect visibleArea=new Rect();Shader beam;float phase;boolean scheduled;
        BloodTrail(SeekBar host,java.util.function.BooleanSupplier playing,float height){this.host=host;this.playing=playing;barHeight=height;density=host.getResources().getDisplayMetrics().density;}
        protected void onBoundsChange(Rect b){float cy=b.exactCenterY();beam=new LinearGradient(0,cy-barHeight/2,0,cy+barHeight/2,new int[]{0xffff8b7c,0xffff3f45,SEEK_START,0xff4d040f},new float[]{0,.18f,.72f,1},Shader.TileMode.CLAMP);}
        boolean animationActive(){return host.isEnabled()&&host.isAttachedToWindow()&&host.isShown()&&host.hasWindowFocus()&&host.getProgress()>0&&playing.getAsBoolean()&&android.animation.ValueAnimator.areAnimatorsEnabled()&&host.getGlobalVisibleRect(visibleArea);}
        public void run(){scheduled=false;if(animationActive()){phase+=.075f;invalidateSelf();}}
        public void draw(Canvas c){Rect b=getBounds();float cy=b.exactCenterY(),top=cy+barHeight*.28f;paint.setStyle(Paint.Style.FILL);paint.setAlpha(255);paint.setShader(beam);c.drawRoundRect(b.left,cy-barHeight/2,b.right,cy+barHeight/2,barHeight/2,barHeight/2,paint);paint.setShader(null);
            // Seven fixed streams, cached gradients and bounded native geometry. No bitmap or seek command per tick.
            for(int i=0;i<7;i++){float cycle=(phase*(.35f+(i%3)*.08f)+i*.173f)%1,x=b.left+b.width()*(i+1)/8f,neck=(1.3f+i%3*.5f)*density;
                float length=(3+i%3*2+(float)Math.sin(Math.min(cycle,.8f)/.8f*Math.PI/2)*(5+i%2*3))*density,bottom=top+length;paint.setColor(SEEK_START);path.rewind();path.moveTo(x-neck*1.5f,top);path.cubicTo(x-neck,top+2*density,x-neck,bottom-2*density,x,bottom);path.cubicTo(x+neck,bottom-2*density,x+neck,top+2*density,x+neck*1.5f,top);path.close();c.drawPath(path,paint);
                paint.setColor(0x66f45e59);paint.setStrokeWidth(.65f*density);c.drawLine(x-neck*.5f,top,x-neck*.25f,bottom-2*density,paint);
                if(cycle>.76f){paint.setColor(SEEK_START);paint.setAlpha(Math.max(0,(int)(255*(1-(cycle-.76f)/.24f))));c.drawOval(x-1.6f*density,bottom+(cycle-.76f)*22*density,x+1.6f*density,bottom+(cycle-.76f)*22*density+4*density,paint);paint.setAlpha(255);}
            }
            if(animationActive()&&!scheduled){scheduled=true;scheduleSelf(this,android.os.SystemClock.uptimeMillis()+67);}else if(!animationActive()&&scheduled){scheduled=false;unscheduleSelf(this);}
        }
        public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
    static final class BloodDrop extends Drawable {
        final int size, color, border; final Paint paint=new Paint(3); final Path path=new Path();Shader shine;
        BloodDrop(int size,int color,int border){this.size=size;this.color=color;this.border=border;}
        protected void onBoundsChange(Rect b){shine=new LinearGradient(0,0,b.width(),size,new int[]{0xffff6264,SEEK_END,SEEK_START,0xff4d040f},new float[]{0,.32f,.72f,1},Shader.TileMode.CLAMP);}
        public void draw(Canvas c){Rect b=getBounds();float w=b.width(),h=b.height();c.save();c.translate(b.left,b.top+size);h=size;path.rewind();path.moveTo(w*.5f,0);path.cubicTo(w*.44f,h*.25f,w*.05f,h*.52f,w*.12f,h*.76f);path.cubicTo(w*.21f,h*1.06f,w*.79f,h*1.06f,w*.88f,h*.76f);path.cubicTo(w*.95f,h*.52f,w*.56f,h*.25f,w*.5f,0);path.close();paint.setStyle(Paint.Style.FILL);paint.setColor(Color.WHITE);paint.setShader(shine);c.drawPath(path,paint);paint.setShader(null);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(1,size*.035f));paint.setColor(SEEK_START);c.drawPath(path,paint);paint.setStyle(Paint.Style.FILL);paint.setColor(0x80ff9b87);c.drawOval(w*.23f,h*.44f,w*.38f,h*.75f,paint);paint.setColor(0x55ff867b);c.drawOval(w*.42f,h*.81f,w*.72f,h*.865f,paint);c.restore();}
        public int getIntrinsicWidth(){return size;}public int getIntrinsicHeight(){return size*2;}public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
    static final class WebOverlay extends Drawable {
        final Paint paint=new Paint(3);final Path p=new Path();public void draw(Canvas c){Rect b=getBounds();float size=Math.min(b.width(),b.height())*.43f;paint.setColor(0xaaaed0cc);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(1.5f);c.save();c.translate(b.left,b.top);for(int i=0;i<=6;i++){double a=i*Math.PI/12;c.drawLine(0,0,(float)Math.cos(a)*size,(float)Math.sin(a)*size,paint);}for(int j=1;j<=4;j++){p.rewind();for(int i=0;i<=6;i++){double a=i*Math.PI/12;float x=(float)Math.cos(a)*size*j/4,y=(float)Math.sin(a)*size*j/4;if(i==0)p.moveTo(x,y);else p.lineTo(x,y);}c.drawPath(p,paint);}c.restore();}public void setAlpha(int a){}public void setColorFilter(ColorFilter f){}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
    /** Mini-player progress: same fill as the theme's seek bar, without a thumb. */
    void progress(ProgressBar bar){
        GradientDrawable track=new GradientDrawable();track.setColor(SEEK_TRACK);track.setCornerRadius(dp(2));
        GradientDrawable fill=new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,new int[]{SEEK_START,SEEK_END});fill.setCornerRadius(dp(2));
        LayerDrawable layers=new LayerDrawable(new Drawable[]{track,new ClipDrawable(fill,Gravity.START,ClipDrawable.HORIZONTAL)});
        layers.setId(0,android.R.id.background);layers.setId(1,android.R.id.progress);bar.setProgressDrawable(layers);bar.setProgressTintList(null);
    }
    ImageButton favorite(boolean liked,Runnable click){ImageButton b=icon(liked?"heart_filled":"heart",liked?"Remover das favoritas":"Favoritar música",click);b.setImageDrawable(new Symbol(liked?"heart_filled":"heart",liked?BLUE:TEXT));return b;}
    static final class Symbol extends Drawable {
        private final String name;private final Paint p=new Paint(3);
        Symbol(String name,int color){this.name=name;p.setColor(color);p.setStrokeWidth(1.8f);p.setStrokeCap(Paint.Cap.ROUND);p.setStrokeJoin(Paint.Join.ROUND);}
        public void draw(Canvas canvas){canvas.save();Rect b=getBounds();float side=Math.min(b.width(),b.height());canvas.translate(b.left+(b.width()-side)/2f,b.top+(b.height()-side)/2f);canvas.scale(side/24f,side/24f);p.setStyle(Paint.Style.STROKE);
            switch(name){
                case "share":canvas.drawCircle(18,5,3,p);canvas.drawCircle(5,12,3,p);canvas.drawCircle(18,19,3,p);canvas.drawLine(8,10.5f,15,6.5f,p);canvas.drawLine(8,13.5f,15,17.5f,p);break;
                case "now":canvas.drawCircle(12,12,9,p);canvas.drawCircle(12,12,3,p);canvas.drawArc(6,6,18,18,210,60,false,p);break;
                case "lyrics":canvas.drawLine(4,5,20,5,p);canvas.drawLine(4,10,17,10,p);canvas.drawLine(4,15,20,15,p);canvas.drawLine(4,20,14,20,p);break;
                case "shake":canvas.save();canvas.rotate(-14,12,12);canvas.drawRoundRect(8,6,16,18,1,1,p);canvas.drawLine(10,9,14,9,p);canvas.drawLine(10,12,14,12,p);canvas.drawLine(10,15,13,15,p);canvas.restore();canvas.drawLine(2,7,1,11,p);canvas.drawLine(1,11,3,15,p);canvas.drawLine(21,7,23,11,p);canvas.drawLine(23,11,22,15,p);break;
                case "random":canvas.drawLine(3,5,7,5,p);canvas.drawLine(7,5,17,19,p);canvas.drawLine(17,19,22,19,p);canvas.drawLine(18,15,22,19,p);canvas.drawLine(22,19,18,23,p);canvas.drawLine(3,19,7,19,p);canvas.drawLine(7,19,17,5,p);canvas.drawLine(17,5,22,5,p);canvas.drawLine(18,1,22,5,p);canvas.drawLine(22,5,18,9,p);break;
                case "discover":canvas.drawCircle(8,8,5.5f,p);canvas.drawLine(4,12,1,16,p);triangle(canvas,7,5.5f,10.5f,8,7,10.5f);canvas.drawLine(19,2,19,8,p);canvas.drawLine(16,5,22,5,p);canvas.drawLine(10,16,13,16,p);canvas.drawLine(13,16,18,22,p);canvas.drawLine(18,22,23,22,p);canvas.drawLine(21,20,23,22,p);canvas.drawLine(10,22,13,22,p);canvas.drawLine(13,22,18,16,p);canvas.drawLine(18,16,23,16,p);canvas.drawLine(21,14,23,16,p);break;
                case "queue":canvas.drawLine(3,5,21,5,p);canvas.drawLine(3,10,21,10,p);canvas.drawLine(3,15,13,15,p);triangle(canvas,17,14,23,18,17,22);break;
                case "up":canvas.drawLine(5,15,12,8,p);canvas.drawLine(12,8,19,15,p);break;
                case "down":canvas.drawLine(5,9,12,16,p);canvas.drawLine(12,16,19,9,p);break;
                case "chevron":canvas.drawLine(8,5,16,12,p);canvas.drawLine(16,12,8,19,p);break;
                case "chart":canvas.drawLine(3,21,21,21,p);for(int i=0;i<3;i++){int x=5+i*6;canvas.drawRoundRect(x,15-i*5,x+3,20,1,1,p);}break;
                case "save":canvas.drawLine(12,3,12,16,p);canvas.drawLine(7,11,12,16,p);canvas.drawLine(12,16,17,11,p);canvas.drawLine(4,18,4,21,p);canvas.drawLine(4,21,20,21,p);canvas.drawLine(20,21,20,18,p);break;
                case "jam":canvas.drawCircle(8,7,3,p);canvas.drawCircle(19,8,2.5f,p);canvas.drawArc(2,13,14,27,180,180,false,p);canvas.drawLine(2,20,14,20,p);canvas.drawArc(14,14,24,26,230,130,false,p);break;
                case "search":canvas.drawCircle(10,10,6,p);canvas.drawLine(15,15,21,21,p);break;
                case "back":canvas.drawLine(15,4,7,12,p);canvas.drawLine(7,12,15,20,p);break;
                case "home":{Path h=new Path();h.moveTo(3,10);h.lineTo(12,3);h.lineTo(21,10);h.lineTo(21,21);h.lineTo(15,21);h.lineTo(15,14);h.lineTo(9,14);h.lineTo(9,21);h.lineTo(3,21);h.close();canvas.drawPath(h,p);break;}
                case "library":canvas.drawLine(4,4,4,21,p);canvas.drawLine(10,4,10,21,p);canvas.drawLine(16,4,21,21,p);break;
                case "pause":p.setStyle(Paint.Style.FILL);canvas.drawRoundRect(6,4,10,20,1,1,p);canvas.drawRoundRect(14,4,18,20,1,1,p);break;
                case "next":case "prev":if(name.equals("prev")){canvas.translate(24,0);canvas.scale(-1,1);}p.setStyle(Paint.Style.FILL);triangle(canvas,5,5,16,12,5,19);canvas.drawRect(17,5,19,19,p);break;
                case "play":p.setStyle(Paint.Style.FILL);triangle(canvas,7,4,20,12,7,20);break;
                case "account":canvas.drawCircle(12,8,4,p);canvas.drawArc(4,14,20,27,185,170,false,p);break;
                case "audio":for(int i=0;i<3;i++){int x=5+i*7,y=7+(i%2)*9;canvas.drawLine(x,3,x,21,p);canvas.drawCircle(x,y,2,p);}break;
                case "more":p.setStyle(Paint.Style.FILL);for(int i=0;i<3;i++)canvas.drawCircle(5+7*i,12,1.5f,p);break;
                case "heart":case "heart_filled":{if(name.equals("heart_filled"))p.setStyle(Paint.Style.FILL);Path h=new Path();h.moveTo(12,21);h.cubicTo(-7,8,6,-2,12,7);h.cubicTo(18,-2,31,8,12,21);canvas.drawPath(h,p);break;}
                case "refresh":canvas.drawArc(4,4,20,20,35,290,false,p);canvas.drawLine(20,3,20,9,p);canvas.drawLine(14,9,20,9,p);break;
                case "expand":canvas.drawLine(4,9,4,4,p);canvas.drawLine(4,4,9,4,p);canvas.drawLine(15,4,20,4,p);canvas.drawLine(20,4,20,9,p);canvas.drawLine(20,15,20,20,p);canvas.drawLine(20,20,15,20,p);canvas.drawLine(9,20,4,20,p);canvas.drawLine(4,20,4,15,p);break;
                case "close":canvas.drawLine(5,5,19,19,p);canvas.drawLine(19,5,5,19,p);break;
                case "translate":canvas.drawLine(3,6,14,6,p);canvas.drawLine(8,3,8,6,p);canvas.drawLine(12,6,6,16,p);canvas.drawLine(5,9,12,15,p);canvas.drawLine(13,21,18,10,p);canvas.drawLine(18,10,23,21,p);canvas.drawLine(15,17,21,17,p);break;
                case "palette":canvas.drawCircle(12,12,9,p);for(int i=0;i<3;i++)canvas.drawCircle(7+i*5,9+(i%2)*5,1,p);break;
                case "font":canvas.drawLine(4,5,20,5,p);canvas.drawLine(12,5,12,21,p);canvas.drawLine(8,21,16,21,p);break;
                case "history":canvas.drawArc(4,4,21,21,220,320,false,p);canvas.drawLine(12,7,12,13,p);canvas.drawLine(12,13,16,15,p);canvas.drawLine(3,3,3,9,p);canvas.drawLine(3,9,9,9,p);break;
                case "trash":canvas.drawLine(4,6,20,6,p);canvas.drawLine(9,3,15,3,p);canvas.drawLine(6,6,7,21,p);canvas.drawLine(7,21,17,21,p);canvas.drawLine(17,21,18,6,p);break;
                case "plus":canvas.drawLine(12,4,12,20,p);canvas.drawLine(4,12,20,12,p);break;
                case "repeat":case "repeat_one":{boolean one=name.equals("repeat_one");if(one){canvas.save();canvas.scale(.72f,.72f,2,12);}
                    canvas.drawLine(5,10,5,8,p);canvas.drawLine(5,8,17,8,p);canvas.drawLine(14,5,17,8,p);canvas.drawLine(17,8,14,11,p);canvas.drawLine(19,14,19,16,p);canvas.drawLine(19,16,7,16,p);canvas.drawLine(10,19,7,16,p);canvas.drawLine(7,16,10,13,p);
                    // Repeat this track: smaller arrows on the left and a readable "1".
                    if(one){canvas.restore();Paint label=new Paint(p);label.setStyle(Paint.Style.FILL);label.setTextSize(11f);label.setTextAlign(Paint.Align.CENTER);label.setFakeBoldText(true);canvas.drawText("1",20f,16.5f,label);}break;}
                case "logo":p.setStyle(Paint.Style.FILL);canvas.drawCircle(12,12,12,p);p.setColor(BG);for(int i=0;i<4;i++){float y=7-Math.abs(i-1.5f)*2;canvas.drawRoundRect(5+i*4,y,7+i*4,24-y,1,1,p);}p.setColor(BLUE);break;
                default:canvas.drawRect(4,4,20,20,p);canvas.drawLine(4,16,10,10,p);canvas.drawLine(10,10,20,19,p);
            }canvas.restore();
        }
        private void triangle(Canvas c,float a,float b,float d,float e,float f,float g){Path path=new Path();path.moveTo(a,b);path.lineTo(d,e);path.lineTo(f,g);path.close();c.drawPath(path,p);}
        public int getIntrinsicWidth(){return Math.round(24*android.content.res.Resources.getSystem().getDisplayMetrics().density);}
        public int getIntrinsicHeight(){return getIntrinsicWidth();}
        public void setAlpha(int a){p.setAlpha(a);}public void setColorFilter(ColorFilter f){p.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
}
