package app.tonal.android;

import android.app.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.function.*;

/** Immersive presentation; one bounded 20 fps render loop, no work in audio callbacks. */
final class LyricFullscreen {
    private final Activity activity;private final MusicStore store;private final Artwork artwork;private final Ui ui;private final BooleanSupplier playing;private final IntSupplier position;private final Runnable closed;
    private final Handler handler=new Handler(Looper.getMainLooper());private Dialog dialog;private LinearLayout rows,controls;private ScrollView scroll;private ImageView image;private LyricBackdrop backdrop;private int version=-1,selected=-2;private String identity="",mode="";private long updated;private boolean visible;
    LyricFullscreen(Activity a,MusicStore store,Artwork art,BooleanSupplier playing,IntSupplier position,Runnable closed){activity=a;this.store=store;artwork=art;this.playing=playing;this.position=position;this.closed=closed;ui=new Ui(a);}
    boolean showing(){return visible;}
    private String mode(){return store.data.optString("background","Aurora");}
    private boolean active(){return visible&&playing.getAsBoolean()&&dialog.getWindow().getDecorView().hasWindowFocus();}
    void show(){dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);FrameLayout root=new FrameLayout(activity);root.setBackgroundColor(0xff242a38);image=new ImageView(activity);image.setScaleType(ImageView.ScaleType.CENTER_CROP);root.addView(image,new FrameLayout.LayoutParams(-1,-1));backdrop=new LyricBackdrop(activity,this::mode,this::active);root.addView(backdrop,new FrameLayout.LayoutParams(-1,-1));View scrim=new View(activity);scrim.setBackgroundColor(0x77202732);root.addView(scrim,new FrameLayout.LayoutParams(-1,-1));rows=ui.column();ui.pad(rows,24,48);scroll=new ScrollView(activity);scroll.setFillViewport(true);scroll.addView(rows);root.addView(scroll,new FrameLayout.LayoutParams(-1,-1));
        controls=ui.column();ui.pad(controls,12,12);ui.rounded(controls,0xed171b24,18);LinearLayout toolbar=ui.row();toolbar.addView(ui.icon("close","Sair da tela cheia",this::dismiss));ui.weight(toolbar,ui.title("Letras",18));toolbar.addView(ui.icon("palette","Efeitos de fundo",()->{handler.removeCallbacks(hide);new AlertDialog.Builder(activity).setTitle("Efeito de fundo").setItems(LyricAppearance.MODES,(d,n)->{MusicStore.put(store.data,"background",LyricAppearance.MODES[n]);store.save();mode="";reveal();}).setOnDismissListener(d->reveal()).show();}));toolbar.addView(ui.icon("font","Fontes e cores",()->{handler.removeCallbacks(hide);LyricAppearance.show(activity,store,()->{version=-1;reveal();});}));controls.addView(toolbar);TextView hint=ui.text("Toque para mostrar ou recolher os controles",12,Ui.MUTED);controls.addView(hint);FrameLayout.LayoutParams cp=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);cp.setMargins(ui.dp(16),0,ui.dp(16),ui.dp(24));root.addView(controls,cp);
        float[] down=new float[2];scroll.setOnTouchListener((v,e)->{if(e.getAction()==MotionEvent.ACTION_DOWN){down[0]=e.getX();down[1]=e.getY();}else if(e.getAction()==MotionEvent.ACTION_UP&&Math.abs(e.getX()-down[0])+Math.abs(e.getY()-down[1])<ui.dp(12)){if(controls.getVisibility()==View.VISIBLE)controls.setVisibility(View.GONE);else reveal();}return false;});
        dialog.setContentView(root);dialog.setOnDismissListener(d->{visible=false;handler.removeCallbacksAndMessages(null);NativeBridge.audio.visualizerActive=false;closed.run();});dialog.show();visible=true;Window window=dialog.getWindow();window.setLayout(-1,-1);if(Build.VERSION.SDK_INT>=30){window.setDecorFitsSystemWindows(false);WindowInsetsController insets=window.getInsetsController();if(insets!=null){insets.hide(WindowInsets.Type.systemBars());insets.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);}}else window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        reveal();handler.post(tick);
    }
    private final Runnable hide=()->controls.setVisibility(View.GONE);
    private void reveal(){controls.setVisibility(View.VISIBLE);handler.removeCallbacks(hide);handler.postDelayed(hide,3500);}
    void dismiss(){if(dialog!=null)dialog.dismiss();}
    private final Runnable tick=new Runnable(){public void run(){if(!visible||activity.isFinishing())return;long now=SystemClock.elapsedRealtime();if(now-updated>=250){updated=now;update();}if(active()&&LyricBackdrop.animated(mode()))backdrop.invalidate();NativeBridge.audio.visualizerActive=active()&&(mode().equals("Ondas")||mode().equals("Espectro"));handler.postDelayed(this,active()&&LyricBackdrop.animated(mode())?50:250);}};
    private void update(){String key=MusicStore.key(store.current);if(!identity.equals(key)||!mode.equals(mode())){identity=key;mode=mode();image.setImageDrawable(null);if(mode.equals("Capa"))artwork.load(image,store.current.optString("image"));else if(mode.equals("Foto"))try{artwork.loadLocal(image,android.net.Uri.parse(store.data.optString("photo")));}catch(Exception ignored){}backdrop.invalidate();version=-1;scroll.scrollTo(0,0);}
        if(version!=store.lyricVersion){version=store.lyricVersion;rows.removeAllViews();JSONObject style=store.lyricStyle();for(Object value:MusicStore.values(store.lines)){JSONObject line=(JSONObject)value;LinearLayout group=ui.column();ui.pad(group,0,16);TextView original=ui.text(line.optString("text"),26,Ui.TEXT);LyricAppearance.apply(original,style,false);group.addView(original);String translated=line.optString("translation");if(!translated.isEmpty()){TextView translation=ui.text(translated,21,Ui.TEXT);LyricAppearance.apply(translation,style,true);group.addView(translation);}rows.addView(group);}ui.space(rows,130);selected=-2;}
        int current=-1;for(int i=0;i<store.lines.length();i++)if(store.lines.optJSONObject(i).optInt("time",-1)>=0&&store.lines.optJSONObject(i).optInt("time")<=position.getAsInt())current=i;
        if(current!=selected){selected=current;for(int i=0;i<store.lines.length();i++)rows.getChildAt(i).setAlpha(current<0||i==current?1f:.55f);if(current>=0&&current<rows.getChildCount()){View row=rows.getChildAt(current);scroll.smoothScrollTo(0,Math.max(0,row.getTop()-scroll.getHeight()/3));}}
    }
}
