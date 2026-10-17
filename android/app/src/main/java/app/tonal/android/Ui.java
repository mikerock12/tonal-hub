package app.tonal.android;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.*;
import android.text.TextUtils;
import android.view.*;
import android.widget.*;

/** Small native view vocabulary shared by the catalog and transport. */
final class Ui {
    static final int BG=0xff101112,CARD=0xff202224,PANEL=0xff18191b,TEXT=0xfff5f5f5,MUTED=0xffa5a8ad,BLUE=0xff009bd6;
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
    ImageButton icon(String name,String description,Runnable click){ImageButton b=new ImageButton(context);b.setImageDrawable(new Symbol(name,TEXT));b.setContentDescription(description);b.setTooltipText(description);b.setBackground(background(0x00000000,24));b.setPadding(dp(12),dp(12),dp(12),dp(12));b.setMinimumWidth(dp(48));b.setMinimumHeight(dp(48));clickable(b,click);return b;}
    ImageButton favorite(boolean liked,Runnable click){ImageButton b=icon(liked?"heart_filled":"heart",liked?"Remover das favoritas":"Favoritar música",click);b.setImageDrawable(new Symbol(liked?"heart_filled":"heart",liked?BLUE:TEXT));return b;}
    static final class Symbol extends Drawable {
        private final String name;private final Paint p=new Paint(3);
        Symbol(String name,int color){this.name=name;p.setColor(color);p.setStrokeWidth(1.8f);p.setStrokeCap(Paint.Cap.ROUND);p.setStrokeJoin(Paint.Join.ROUND);}
        public void draw(Canvas canvas){canvas.save();Rect b=getBounds();canvas.translate(b.left,b.top);canvas.scale(b.width()/24f,b.height()/24f);p.setStyle(Paint.Style.STROKE);
            switch(name){
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
                case "logo":p.setStyle(Paint.Style.FILL);canvas.drawCircle(12,12,12,p);p.setColor(BG);for(int i=0;i<4;i++){float y=7-Math.abs(i-1.5f)*2;canvas.drawRoundRect(5+i*4,y,7+i*4,24-y,1,1,p);}p.setColor(BLUE);break;
                default:canvas.drawRect(4,4,20,20,p);canvas.drawLine(4,16,10,10,p);canvas.drawLine(10,10,20,19,p);
            }canvas.restore();
        }
        private void triangle(Canvas c,float a,float b,float d,float e,float f,float g){Path path=new Path();path.moveTo(a,b);path.lineTo(d,e);path.lineTo(f,g);path.close();c.drawPath(path,p);}
        public void setAlpha(int a){p.setAlpha(a);}public void setColorFilter(ColorFilter f){p.setColorFilter(f);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    }
}
