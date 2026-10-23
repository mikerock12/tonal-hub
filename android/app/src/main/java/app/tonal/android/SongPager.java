package app.tonal.android;

import android.animation.ValueAnimator;
import android.content.Context;
import android.view.*;
import android.widget.ScrollView;
import java.util.function.IntConsumer;

/** Three bounded native pages. Inner lyrics keep their scroll; at an edge the
 * next gesture changes page. No timer/animation runs when the pager is at rest. */
final class SongPager extends ViewGroup {
    private float downX,downY,startY; private boolean dragging;
    private final int slop; private ValueAnimator animation; int page;
    IntConsumer changed=n->{};
    SongPager(Context c){super(c);slop=ViewConfiguration.get(c).getScaledTouchSlop();setClipChildren(true);}
    void showPage(int index,boolean animate){
        page=Math.max(0,Math.min(getChildCount()-1,index));
        if(animation!=null)animation.cancel();
        int end=page*getHeight();
        if(animate&&isLaidOut()){
            animation=ValueAnimator.ofInt(getScrollY(),end);animation.setDuration(260);
            animation.setInterpolator(new android.view.animation.DecelerateInterpolator());
            animation.addUpdateListener(a->scrollTo(0,(int)a.getAnimatedValue()));animation.start();
        }else scrollTo(0,end);
        changed.accept(page);announceForAccessibility(new String[]{"Música e controles","Letra e tradução","Sobre o artista"}[page]);
    }
    @Override protected void onMeasure(int w,int h){super.onMeasure(w,h);for(int i=0;i<getChildCount();i++)getChildAt(i).measure(MeasureSpec.makeMeasureSpec(getMeasuredWidth(),MeasureSpec.EXACTLY),MeasureSpec.makeMeasureSpec(getMeasuredHeight(),MeasureSpec.EXACTLY));}
    @Override protected void onLayout(boolean changed,int l,int t,int r,int b){int h=b-t;for(int i=0;i<getChildCount();i++)getChildAt(i).layout(0,i*h,r-l,(i+1)*h);if(changed)scrollTo(0,page*h);}
    private boolean innerCanScroll(View view,int direction){
        if(view instanceof ScrollView&&view.canScrollVertically(direction))return true;
        if(view instanceof ViewGroup){ViewGroup g=(ViewGroup)view;for(int i=0;i<g.getChildCount();i++)if(innerCanScroll(g.getChildAt(i),direction))return true;}
        return false;
    }
    @Override public void requestDisallowInterceptTouchEvent(boolean disallow){
        // ScrollView asks its ancestors to stop observing as soon as it drags.
        // Keep observing here so an edge gesture can pass to the next page.
        // onInterceptTouchEvent still leaves all available inner scrolling and
        // predominantly horizontal seek gestures with their original child.
        super.requestDisallowInterceptTouchEvent(false);
    }
    @Override public boolean onInterceptTouchEvent(android.view.MotionEvent e){
        if(e.getActionMasked()==MotionEvent.ACTION_DOWN){downX=e.getX();downY=e.getY();startY=getScrollY();dragging=false;return false;}
        if(e.getActionMasked()==MotionEvent.ACTION_MOVE){float dy=e.getY()-downY;int direction=dy<0?1:-1;
            if(Math.abs(dy)>slop*2&&Math.abs(dy)>Math.abs(e.getX()-downX)*1.4f&&!innerCanScroll(getChildAt(page),direction)){
                if(animation!=null)animation.cancel();dragging=true;getParent().requestDisallowInterceptTouchEvent(true);return true;}}
        return false;
    }
    @Override public boolean onTouchEvent(MotionEvent e){
        if(e.getActionMasked()==MotionEvent.ACTION_MOVE&&dragging){scrollTo(0,(int)Math.max(0,Math.min((getChildCount()-1)*getHeight(),startY+downY-e.getY())));return true;}
        if(e.getActionMasked()==MotionEvent.ACTION_UP||e.getActionMasked()==MotionEvent.ACTION_CANCEL){
            float delta=downY-e.getY();int next=page;
            if(e.getActionMasked()!=MotionEvent.ACTION_CANCEL&&Math.abs(delta)>Math.max(slop*3,getHeight()*.12f))next+=delta>0?1:-1;
            showPage(next,true);dragging=false;return true;
        }return true;
    }
    @Override protected void onDetachedFromWindow(){if(animation!=null)animation.cancel();super.onDetachedFromWindow();}
}
