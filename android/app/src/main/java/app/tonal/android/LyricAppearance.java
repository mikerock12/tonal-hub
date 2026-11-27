package app.tonal.android;

import android.app.*;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.Layout;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import org.json.*;
import java.util.Locale;

final class LyricAppearance {
    static final String[] MODES={"Aurora","Ondas","Espectro","Capa","Foto","Estrelas","Chuva","Orbitas","Pulso","Grade","Halloween","Bruxas","Monstros","Vampiros","Natal","Neve","Lareira","Sem animação"};
    // Stored names (preferences/backup) — order is the menu order. System families only: nothing is bundled or downloaded.
    static final String[] FONTS={"Sans","Serif","Mono","Rounded","Fina","Média","Negra","Condensada","Casual","Cursiva","Versalete","Máquina"};
    static final String[] FONT_LABELS={"Sans","Serif","Mono","Arredondada","Fina","Média","Negra","Condensada","Casual","Cursiva","Versalete","Máquina de escrever"};
    static final String[] EFFECTS={"Normal","Contorno","Contorno grosso","Brilho","Neon","Sombra","Relevo","3D","Vazado","Marca-texto","Gradiente","Piscante"};
    static Typeface font(String name){
        String family;switch(name==null?"":name){
            case "Serif":family="serif";break;case "Mono":family="monospace";break;case "Rounded":family="sans-serif-rounded";break;
            case "Fina":family="sans-serif-light";break;case "Média":family="sans-serif-medium";break;case "Negra":family="sans-serif-black";break;
            case "Condensada":family="sans-serif-condensed";break;case "Casual":family="casual";break;case "Cursiva":family="cursive";break;
            case "Versalete":family="sans-serif-smallcaps";break;case "Máquina":family="serif-monospace";break;default:family="sans-serif";
        }
        return Typeface.create(family,Typeface.NORMAL);
    }
    static String fontLabel(String name){for(int i=0;i<FONTS.length;i++)if(FONTS[i].equals(name))return FONT_LABELS[i];return name;}

    /** Lyric line with the stroke/shadow passes the effects need. Paint objects are reused; nothing allocates per frame. */
    static final class Verse extends TextView {
        String effect="Normal";int outline=0xff101722,ink=0xfff5f5f5;private final Paint mark=new Paint(Paint.ANTI_ALIAS_FLAG);private final RectF line=new RectF();private LinearGradient gradient;private int gradientKey;
        Verse(Activity a){super(a);setIncludeFontPadding(true);}
        /** Same vertical placement TextView applies to its layout (gravity), so extra passes line up with the text. */
        private float top(){
            Layout l=getLayout();float top=getExtendedPaddingTop();if(l==null)return top;int box=getMeasuredHeight()-getExtendedPaddingTop()-getExtendedPaddingBottom(),text=l.getHeight();
            if(text>=box)return top;int gravity=getGravity()&Gravity.VERTICAL_GRAVITY_MASK;return top+(gravity==Gravity.BOTTOM?box-text:gravity==Gravity.CENTER_VERTICAL?(box-text)/2f:0);
        }
        private void layoutPass(Canvas c,int color,Paint.Style style,float width,float dx,float dy){
            if(getLayout()==null)return;Paint p=getPaint();p.setStyle(style);p.setStrokeJoin(Paint.Join.ROUND);p.setStrokeWidth(width);p.setColor(color);
            int checkpoint=c.save();c.translate(getCompoundPaddingLeft()+dx,top()+dy);getLayout().draw(c);c.restoreToCount(checkpoint);p.setStyle(Paint.Style.FILL);
        }
        protected void onDraw(Canvas c){
            float d=getResources().getDisplayMetrics().density;
            switch(effect){
                case "Contorno":case "Neon":layoutPass(c,outline,Paint.Style.STROKE,d*2,0,0);break;
                case "Contorno grosso":layoutPass(c,outline,Paint.Style.STROKE,d*4.5f,0,0);break;
                case "Relevo":layoutPass(c,0x55ffffff,Paint.Style.FILL,0,-d*.6f,-d*.6f);layoutPass(c,outline,Paint.Style.FILL,0,d,d);break;
                case "3D":for(int k=4;k>=1;k--)layoutPass(c,outline,Paint.Style.FILL,0,d*k,d*k);break;
                case "Marca-texto":
                    if(getLayout()!=null){mark.setColor((outline&0x00ffffff)|0xb0000000);Layout layout=getLayout();float left=getCompoundPaddingLeft(),top=top();
                        for(int i=0;i<layout.getLineCount();i++){line.set(left+layout.getLineLeft(i)-d*4,top+layout.getLineTop(i)+d,left+layout.getLineRight(i)+d*4,top+layout.getLineBottom(i)-d);c.drawRoundRect(line,d*4,d*4,mark);}}
                    break;
                case "Vazado":layoutPass(c,ink,Paint.Style.STROKE,d*1.5f,0,0);break;
                default:break;
            }
            if(effect.equals("Gradiente")){int key=getHeight()*31+ink*17+outline;if(gradient==null||key!=gradientKey){gradient=new LinearGradient(0,getExtendedPaddingTop(),0,Math.max(1,getHeight()-getExtendedPaddingBottom()),ink,outline,Shader.TileMode.CLAMP);gradientKey=key;}getPaint().setShader(gradient);super.onDraw(c);getPaint().setShader(null);return;}
            super.onDraw(c);
        }
    }
    static Verse verse(Activity a,String text){Verse v=new Verse(a);v.setText(text);return v;}
    static int parse(String color,int fallback){try{return Color.parseColor(color);}catch(RuntimeException invalid){return fallback;}}
    static void apply(TextView view,JSONObject style,boolean translation){String prefix=translation?"translation":"lyric";applyLook(view,style.optString(prefix+"Font"),style.optString(prefix+"Effect","Normal"),parse(style.optString(prefix+"Color"),0xfff5f5f5),parse(style.optString(prefix+"OutlineColor","#101722"),0xff101722),style.optBoolean(prefix+"Bold"),style.optBoolean(prefix+"Italic"),style.optBoolean(prefix+"Underline"));view.setTextSize(style.optInt(prefix+"Size"));view.invalidate();}
    static void applyLook(TextView view,String font,String effect,int color,int outline,boolean bold,boolean italic,boolean underline){
        int face=(bold?Typeface.BOLD:0)|(italic?Typeface.ITALIC:0);view.setTypeface(font(font),face);
        boolean hollow=effect.equals("Vazado")&&view instanceof Verse;view.setTextColor(hollow?Color.TRANSPARENT:color);
        view.setPaintFlags(underline?view.getPaintFlags()|Paint.UNDERLINE_TEXT_FLAG:view.getPaintFlags()&~Paint.UNDERLINE_TEXT_FLAG);
        float d=view.getResources().getDisplayMetrics().density;
        if(effect.equals("Brilho")||effect.equals("Neon"))view.setShadowLayer(6*d,0,0,outline);
        else if(effect.equals("Sombra"))view.setShadowLayer(4*d,3*d,3*d,outline);
        else view.setShadowLayer(0,0,0,0);
        if(view instanceof Verse){((Verse)view).effect=effect;((Verse)view).outline=outline;((Verse)view).ink=color;}
    }
    static void animate(TextView v,JSONObject style,boolean translation,boolean playing,long now){v.setAlpha(playing&&style.optString((translation?"translation":"lyric")+"Effect").equals("Piscante")?(now/650%2==0?1f:.4f):1f);}

    // ---- rainbow rail: 0..1000 over 16 color stops, from white through the spectrum to black (LyricRail) ----------
    static final int RAIL_MAX=LyricRail.MAX;
    static int railColor(int position){return LyricRail.color(position);}
    static String hex(int color){return LyricRail.hex(color);}
    static int nearestRail(int color){return LyricRail.nearest(color);}
    private static final int[] STOPS=LyricRail.STOPS;
    static GradientDrawable railDrawable(Ui ui){int[] colors=new int[STOPS.length];for(int i=0;i<STOPS.length;i++)colors[i]=0xff000000|STOPS[i];GradientDrawable g=new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,colors);g.setCornerRadius(ui.dp(8));g.setStroke(1,0x66ffffff);return g;}

    /** "−  [arco-íris]  +" with 1001 positions; hold −/+ to repeat. Applies on release and on each −/+ step. */
    private static View colorRail(Activity a,Ui ui,MusicStore store,String key,String label,Runnable changed){
        LinearLayout box=ui.column();JSONObject style=store.lyricStyle();int stored=parse(style.optString(key),0xffffffff),start=nearestRail(stored);
        LinearLayout head=ui.row();TextView title=ui.text(label,14,Ui.MUTED);ui.weight(head,title);View swatch=new View(a);GradientDrawable chip=new GradientDrawable();chip.setCornerRadius(ui.dp(6));chip.setStroke(1,0x88ffffff);swatch.setBackground(chip);head.addView(swatch,new LinearLayout.LayoutParams(ui.dp(28),ui.dp(20)));
        TextView value=ui.text("",13,Ui.TEXT);value.setTag(key+"-value");ui.pad(value,8,0);head.addView(value);box.addView(head);
        LinearLayout row=ui.row();row.setGravity(Gravity.CENTER_VERTICAL);SeekBar rail=new SeekBar(a);rail.setTag(key+"-rail");rail.setMax(RAIL_MAX);rail.setProgress(start);rail.setProgressDrawable(railDrawable(ui));rail.setContentDescription(label+": posição de 0 a 1000 no arco-íris");rail.setPadding(ui.dp(14),ui.dp(10),ui.dp(14),ui.dp(10));
        Runnable show=()->{int c=railColor(rail.getProgress());chip.setColor(c);value.setText(hex(c)+" · "+rail.getProgress());};
        Runnable commit=()->{store.setLyricStyle(key,hex(railColor(rail.getProgress())));changed.run();};
        rail.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar b,int v,boolean user){show.run();}public void onStartTrackingTouch(SeekBar b){}public void onStopTrackingTouch(SeekBar b){commit.run();}});
        row.addView(stepper(a,ui,"−",label+": menos 1",rail,-1,commit));ui.weight(row,rail);row.addView(stepper(a,ui,"+",label+": mais 1",rail,1,commit));box.addView(row);
        // Until the rail is touched, show the stored color itself (older colors sit between rail positions).
        chip.setColor(stored);value.setText(hex(stored)+" · "+start);return box;
    }
    private static TextView stepper(Activity a,Ui ui,String symbol,String description,SeekBar rail,int delta,Runnable commit){
        TextView b=ui.text(symbol,22,Ui.TEXT);b.setGravity(Gravity.CENTER);b.setContentDescription(description);b.setTag(description);GradientDrawable bg=new GradientDrawable();bg.setCornerRadius(ui.dp(18));bg.setColor(0xff263440);b.setBackground(bg);b.setLayoutParams(new LinearLayout.LayoutParams(ui.dp(40),ui.dp(40)));
        Handler handler=new Handler(Looper.getMainLooper());Runnable[] repeat=new Runnable[1];
        repeat[0]=()->{rail.setProgress(Math.max(0,Math.min(RAIL_MAX,rail.getProgress()+delta)));handler.postDelayed(repeat[0],60);};
        b.setClickable(true);b.setOnTouchListener((v,e)->{switch(e.getActionMasked()){
            case MotionEvent.ACTION_DOWN:v.setPressed(true);rail.setProgress(Math.max(0,Math.min(RAIL_MAX,rail.getProgress()+delta)));handler.postDelayed(repeat[0],400);return true;
            case MotionEvent.ACTION_UP:v.performClick();case MotionEvent.ACTION_CANCEL:v.setPressed(false);handler.removeCallbacks(repeat[0]);commit.run();return true;
            default:return true;}});
        b.setOnClickListener(v->{});return b;
    }
    /** Spinner whose rows are drawn with the option itself: each font in its typeface, each effect applied. */
    private static Spinner preview(Activity a,Ui ui,String[] values,String[] labels,String current,boolean fonts,MusicStore store,String prefix,java.util.function.Consumer<String> chosen){
        Spinner spinner=new Spinner(a);
        ArrayAdapter<String> adapter=new ArrayAdapter<String>(a,0,values){
            View row(int position,View convert,boolean dropDown){
                Verse v=convert instanceof Verse?(Verse)convert:new Verse(a);String value=values[position];JSONObject style=store.lyricStyle();int color=parse(style.optString(prefix+"Color"),0xfff5f5f5),outline=parse(style.optString(prefix+"OutlineColor","#101722"),0xff101722);
                v.setText(fonts?labels[position]+" · Aa Bb Çç 123":labels[position]);v.setTextSize(dropDown?20:18);v.setSingleLine(true);v.setPadding(ui.dp(16),ui.dp(dropDown?12:8),ui.dp(16),ui.dp(dropDown?12:8));v.setMinHeight(ui.dp(dropDown?56:44));v.setGravity(Gravity.CENTER_VERTICAL);
                v.setContentDescription((fonts?"Fonte ":"Efeito ")+labels[position]);
                if(fonts)applyLook(v,value,"Normal",Ui.TEXT,outline,false,false,false);else applyLook(v,style.optString(prefix+"Font"),value,color,outline,style.optBoolean(prefix+"Bold"),style.optBoolean(prefix+"Italic"),false);
                v.setAlpha(1f);v.setBackgroundColor(dropDown&&value.equals(style.optString(prefix+(fonts?"Font":"Effect")))?0x3363d5ff:Color.TRANSPARENT);return v;
            }
            public View getView(int position,View convert,ViewGroup parent){return row(position,convert,false);}
            public View getDropDownView(int position,View convert,ViewGroup parent){return row(position,convert,true);}
        };
        spinner.setAdapter(adapter);for(int i=0;i<values.length;i++)if(values[i].equals(current))spinner.setSelection(i);
        final int initial=spinner.getSelectedItemPosition();
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){int last=initial;public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int n,long id){if(n!=last){last=n;chosen.accept(values[n]);}}});
        return spinner;
    }
    static void show(Activity a,MusicStore store,Runnable changed){
        Ui ui=new Ui(a);LinearLayout body=ui.column();ui.pad(body,18,12);JSONObject style=store.lyricStyle();
        for(String prefix:new String[]{"lyric","translation"}){
            String who=prefix.equals("lyric")?"letra":"tradução";body.addView(ui.title(prefix.equals("lyric")?"Letra original":"Tradução",20));
            body.addView(ui.text("Fonte",14,Ui.MUTED));
            Spinner font=preview(a,ui,FONTS,FONT_LABELS,style.optString(prefix+"Font"),true,store,prefix,value->{store.setLyricStyle(prefix+"Font",value);changed.run();});font.setTag(prefix+"-font");font.setContentDescription("Fonte da "+who);body.addView(font);
            body.addView(colorRail(a,ui,store,prefix+"Color","Cor do texto",()->{changed.run();}));
            body.addView(ui.text("Tamanho",14,Ui.MUTED));
            SeekBar size=new SeekBar(a);ui.seek(size);size.setMax(28);size.setProgress(style.optInt(prefix+"Size")-16);size.setContentDescription("Tamanho da "+who);size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar b,int value,boolean user){if(user){store.setLyricStyle(prefix+"Size",16+value);changed.run();}}public void onStartTrackingTouch(SeekBar b){}public void onStopTrackingTouch(SeekBar b){}});body.addView(size);
            body.addView(colorRail(a,ui,store,prefix+"OutlineColor","Contorno / brilho / sombra",()->{changed.run();}));
            body.addView(ui.text("Efeito",14,Ui.MUTED));
            Spinner effect=preview(a,ui,EFFECTS,EFFECTS,style.optString(prefix+"Effect","Normal"),false,store,prefix,value->{store.setLyricStyle(prefix+"Effect",value);changed.run();});effect.setTag(prefix+"-effect");effect.setContentDescription("Efeito da "+who);body.addView(effect);
            LinearLayout flags=ui.row();for(String flag:new String[]{"Bold","Italic","Underline"}){CheckBox check=new CheckBox(a);check.setText(flag.equals("Bold")?"Negrito":flag.equals("Italic")?"Itálico":"Sublinhar");check.setTextColor(Ui.TEXT);check.setTextSize(12);check.setChecked(style.optBoolean(prefix+flag));check.setOnCheckedChangeListener((v,on)->{store.setLyricStyle(prefix+flag,on);changed.run();});ui.weight(flags,check);}body.addView(flags);ui.space(body,20);
        }
        ScrollView scroll=new ScrollView(a);scroll.addView(body);
        // Menu previews re-read the stored style when opened, so they follow the colors just chosen.
        new AlertDialog.Builder(a).setTitle("Fontes e cores das letras").setView(scroll).setPositiveButton("Concluir",null).show();
    }
}
