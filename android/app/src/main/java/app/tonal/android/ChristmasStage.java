package app.tonal.android;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ImageDecoder;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Drawable;
import android.os.Process;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import java.util.function.BooleanSupplier;

/**
 * Player cover wrapped in the garland: back layer, cover, front layer, lit bulbs, Santa.
 * Wire, sockets and shadows are baked, so a tick only changes bulb sprite alphas. Santa
 * decodes in the platform's animation thread, one frame ahead, with flat memory use.
 */
final class ChristmasStage extends FrameLayout implements Runnable {
    static final float SANTA_SHARE=.46f;
    final ImageView cover;
    private final BooleanSupplier playing;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
    private final Matrix matrix=new Matrix();final RectF art=new RectF();private final Rect area=new Rect(),target=new Rect();
    private ChristmasAssets assets;AnimatedImageDrawable santa;boolean running;float phase;

    ChristmasStage(Context context,ImageView cover,BooleanSupplier playing,int overhang){
        super(context);this.cover=cover;this.playing=playing;setWillNotDraw(false);setClipChildren(false);setClipToPadding(false);
        LayoutParams params=new LayoutParams(-1,-1);params.setMargins(overhang,overhang,overhang,overhang);addView(cover,params);
        assets=ChristmasAssets.get(context,()->{assets=ChristmasAssets.get(context,null);invalidate();});
        Context app=context.getApplicationContext();
        Thread worker=new Thread(()->{
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
            try{Drawable decoded=ImageDecoder.decodeDrawable(ImageDecoder.createSource(app.getAssets(),"seasonal/santa.webp"));
                if(decoded instanceof AnimatedImageDrawable)post(()->attachSanta((AnimatedImageDrawable)decoded));
            }catch(Exception|OutOfMemoryError error){Diagnostics.write("Natal: animação indisponível");}
        },"TonalChristmasSanta");worker.start();
    }
    private void attachSanta(AnimatedImageDrawable drawable){if(!isAttachedToWindow())return;santa=drawable;santa.setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);santa.setCallback(this);if(running)santa.start();invalidate();}
    @Override protected boolean verifyDrawable(Drawable who){return who==santa||super.verifyDrawable(who);}
    @Override protected void onAttachedToWindow(){super.onAttachedToWindow();post(this);}
    @Override protected void onDetachedFromWindow(){removeCallbacks(this);ChristmasLights.leave(tick);if(santa!=null){santa.stop();santa.setCallback(null);santa=null;}running=false;super.onDetachedFromWindow();}

    private boolean shouldRun(){return playing.getAsBoolean()&&SeasonalDrawables.visible(this,area)&&ChristmasLights.allowed(getContext());}
    @Override public void run(){
        boolean next=shouldRun();
        if(next!=running){running=next;if(santa!=null){if(next)santa.start();else santa.stop();}}
        else if(next&&santa!=null&&!santa.isRunning())santa.start();
        // One shared clock: separate timers would each redraw the whole window out of phase.
        // Stopped, this is just a cheap 2 Hz look at whether it may resume.
        if(next)ChristmasLights.join(tick);else{ChristmasLights.leave(tick);postDelayed(this,500);}
    }
    private final Runnable tick=new Runnable(){public void run(){
        if(shouldRun()){phase+=ChristmasLights.STEP;invalidate();}else{ChristmasLights.leave(this);ChristmasStage.this.run();}
    }};
    /** Painted cover area in this view's coordinates; covers need not fill their box. */
    private void coverArea(){
        Drawable image=cover.getDrawable();float left=cover.getLeft(),top=cover.getTop();
        if(image!=null&&image.getIntrinsicWidth()>0&&image.getIntrinsicHeight()>0){art.set(0,0,image.getIntrinsicWidth(),image.getIntrinsicHeight());cover.getImageMatrix().mapRect(art);art.offset(left+cover.getPaddingLeft(),top+cover.getPaddingTop());}
        else{float side=Math.min(cover.getWidth(),cover.getHeight());art.set(0,0,side,side);art.offset(left+(cover.getWidth()-side)/2,top+(cover.getHeight()-side)/2);}
    }
    private void layer(Canvas canvas,Bitmap bitmap){
        float grow=assets.margin,w=art.width(),h=art.height();paint.setAlpha(255);
        matrix.setScale(w*(1+2*grow)/bitmap.getWidth(),h*(1+2*grow)/bitmap.getHeight());matrix.postTranslate(art.left-w*grow,art.top-h*grow);canvas.drawBitmap(bitmap,matrix,paint);
    }
    private void bulbs(Canvas canvas,boolean front){
        ChristmasAssets.Bulb[] rows=assets.bulbs;float side=art.width()*assets.length*assets.span;
        for(int i=0;i<rows.length;i++){
            ChristmasAssets.Bulb b=rows[i];if(b.front!=front)continue;
            Bitmap sprite=assets.on[b.color];float scale=side/sprite.getWidth();
            paint.setAlpha((int)(255*ChristmasLights.level(i,phase)));
            matrix.setTranslate(-sprite.getWidth()/2f,-sprite.getHeight()/2f);matrix.postScale(scale,scale);matrix.postRotate(b.angle);matrix.postTranslate(art.left+b.x*art.width(),art.top+b.y*art.height());
            canvas.drawBitmap(sprite,matrix,paint);
        }
    }
    @Override protected void dispatchDraw(Canvas canvas){
        coverArea();boolean ready=assets!=null&&art.width()>0;
        if(ready){layer(canvas,assets.back);bulbs(canvas,false);}
        super.dispatchDraw(canvas);
        if(ready){layer(canvas,assets.front);bulbs(canvas,true);}
        if(santa!=null&&art.width()>0){
            int side=Math.round(art.width()*SANTA_SHARE),right=Math.round(art.right+art.width()*.008f),bottom=Math.round(art.bottom+art.width()*.006f);
            target.set(right-side,bottom-side,right,bottom);if(!santa.getBounds().equals(target))santa.setBounds(target);santa.draw(canvas);
        }
    }

    /** Holds the cover in its usual place while the layers reach into the card's padding. */
    static void mount(ViewGroup card,View stage,ViewGroup.MarginLayoutParams params,int overhang){
        params.setMargins(-overhang,-overhang,-overhang,-overhang);if(params.height>0)params.height+=2*overhang;
        card.setClipToPadding(false);card.setClipChildren(false);card.addView(stage,params);
    }
}
