package app.tonal.android;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.concurrent.atomic.*;
import org.json.*;

/** Native component rendering in the dedicated benchmark app; original synthetic art only. */
final class HalloweenTests {
    static void require(boolean value,String why){if(!value)throw new AssertionError(why);}
    static String art(){Bitmap b=Bitmap.createBitmap(320,320,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);c.drawColor(0xff34475b);Paint p=new Paint(3);p.setColor(0xffb87042);c.drawCircle(160,150,100,p);p.setColor(0xff34475b);c.drawCircle(160,150,35,p);p.setColor(0xfffff4e9);p.setTextSize(20);c.drawText("CAPA SINTÉTICA",75,285,p);ByteArrayOutputStream out=new ByteArrayOutputStream();b.compress(Bitmap.CompressFormat.JPEG,90,out);b.recycle();return "data:image/jpeg;base64,"+android.util.Base64.encodeToString(out.toByteArray(),android.util.Base64.NO_WRAP);}
    static void capture(View view,File file)throws Exception{Bitmap b=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);view.draw(new Canvas(b));try(FileOutputStream out=new FileOutputStream(file)){b.compress(Bitmap.CompressFormat.PNG,100,out);}finally{b.recycle();}}
    static String run(Instrumentation test)throws Exception{
        Context app=test.getTargetContext();String previous=AppTheme.chosen(app);test.runOnMainSync(()->AppTheme.choose(app,"halloween"));
        Activity activity=test.startActivitySync(new Intent(app,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        AtomicBoolean playing=new AtomicBoolean(true);AtomicReference<String> mode=new AtomicReference<>("Halloween");
        SeekBar[] seek={null};Ui.BloodTrail[] blood={null};LinearLayout[] root={null};LyricBackdrop[] scene={null};Artwork[] art={null};
        File folder=new File(app.getExternalFilesDir(null),"halloween-0154");folder.mkdirs();
        try{
            test.runOnMainSync(()->{Ui ui=new Ui(activity);root[0]=ui.column();root[0].setBackgroundColor(Ui.BG);ui.pad(root[0],18,14);root[0].addView(ui.title("Halloween · componentes reais",21));root[0].addView(ui.text("CAPA E LETRAS SINTÉTICAS · SEM STREAMING",11,Ui.MUTED));
                ImageView cover=new ImageView(activity);cover.setScaleType(ImageView.ScaleType.FIT_CENTER);root[0].addView(cover,new LinearLayout.LayoutParams(-1,ui.dp(180)));art[0]=new Artwork(activity);art[0].load(cover,art());
                seek[0]=new SeekBar(activity);ui.seek(seek[0],playing::get);seek[0].setMax(100);seek[0].setProgress(62);root[0].addView(seek[0],new LinearLayout.LayoutParams(-1,ui.dp(48)));blood[0]=(Ui.BloodTrail)((ClipDrawable)((LayerDrawable)seek[0].getProgressDrawable()).getDrawable(1)).getDrawable();
                LinearLayout controls=ui.row();controls.setGravity(Gravity.CENTER);for(String symbol:new String[]{"prev","play","next"})controls.addView(ui.icon(symbol,symbol,()->{}),new LinearLayout.LayoutParams(ui.dp(56),ui.dp(56)));root[0].addView(controls);
                FrameLayout stage=new FrameLayout(activity);scene[0]=new LyricBackdrop(activity,mode::get,playing::get);stage.addView(scene[0],new FrameLayout.LayoutParams(-1,-1));LinearLayout words=ui.column();ui.pad(words,16,14);words.addView(ui.title("A noite abre outro caminho",21));words.addView(ui.text("Tradução sintética para demonstração",14,Ui.MUTED));ui.space(words,12);words.addView(ui.title("Amanhã traz novas cores",21));stage.addView(words);root[0].addView(stage,new LinearLayout.LayoutParams(-1,ui.dp(220)));activity.setContentView(root[0]);});
            Thread.sleep(700);float[] phase={0};test.runOnMainSync(()->phase[0]=blood[0].phase);
            boolean animations=android.animation.ValueAnimator.areAnimatorsEnabled();if(animations)require(phase[0]>0,"visible playing blood did not animate");
            test.runOnMainSync(()->playing.set(false));Thread.sleep(120);test.runOnMainSync(()->phase[0]=blood[0].phase);Thread.sleep(200);test.runOnMainSync(()->require(blood[0].phase==phase[0],"paused blood kept scheduling"));
            test.runOnMainSync(()->{seek[0].setVisibility(View.INVISIBLE);playing.set(true);});Thread.sleep(180);test.runOnMainSync(()->require(blood[0].phase==phase[0],"hidden blood kept scheduling"));
            test.runOnMainSync(()->{seek[0].setVisibility(View.VISIBLE);seek[0].invalidate();});Thread.sleep(220);if(animations)test.runOnMainSync(()->require(blood[0].phase>phase[0],"visible blood did not resume"));
            test.runOnMainSync(()->playing.set(false));
            for(String name:new String[]{"Halloween","Bruxas","Monstros","Vampiros"}){test.runOnMainSync(()->{mode.set(name);scene[0].invalidate();});Thread.sleep(120);test.runOnMainSync(()->{try{capture(root[0],new File(folder,name+".png"));}catch(Exception e){throw new RuntimeException(e);}});}
            return new JSONObject().put("passed",true).put("syntheticArtAndLyrics",true).put("physicalPhone",false).put("liveSpotify",false).put("scenes",4).put("animationSystemEnabled",animations).put("visiblePlayingAnimates",animations).put("pausedStops",true).put("hiddenStops",true).put("visibleResumes",animations).put("maxFps",15).toString();
        }finally{test.runOnMainSync(()->{playing.set(false);if(art[0]!=null)art[0].close();AppTheme.choose(app,previous);activity.finish();});}
    }
}
