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

final class SeasonalTests {
    private static Bitmap bitmap(View view){Bitmap b=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);view.draw(new Canvas(b));return b;}
    private static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static String run(Instrumentation test)throws Exception{
        Context app=test.getTargetContext();String previous=AppTheme.chosen(app);Activity activity=test.startActivitySync(new Intent(app,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        File folder=new File(app.getExternalFilesDir(null),"seasonal-0157");folder.mkdirs();AtomicBoolean playing=new AtomicBoolean(true);AtomicReference<String> mode=new AtomicReference<>("Natal");
        try{
            test.runOnMainSync(()->AppTheme.choose(app,"halloween"));
            ImageButton[] halloweenButton={null};test.runOnMainSync(()->{Ui ui=new Ui(activity);halloweenButton[0]=ui.icon("play","Reproduzir",()->{});activity.setContentView(halloweenButton[0],new ViewGroup.LayoutParams(ui.dp(64),ui.dp(64)));});
            test.runOnMainSync(()->new Ui(activity).setTransport(halloweenButton[0],true));Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)halloweenButton[0].getDrawable();require(glyph.playing&&glyph.mix==1,"Halloween play-to-pause morph");new Ui(activity).setTransport(halloweenButton[0],false);});Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)halloweenButton[0].getDrawable();require(!glyph.playing&&glyph.mix==0,"Halloween pause-to-play morph");require(Ui.ON_ACCENT!=Ui.BLUE,"collection play icon must differ from its background");});
            test.runOnMainSync(()->AppTheme.choose(app,"natal"));
            LinearLayout[] root={null};ImageView[] cover={null};SeekBar[] seek={null};ImageButton[] toggle={null};LyricBackdrop[] scene={null};
            test.runOnMainSync(()->{Ui ui=new Ui(activity);root[0]=ui.column();root[0].setBackgroundColor(Ui.BG);ui.pad(root[0],18,12);root[0].addView(ui.title("Natal · componentes reais",21));root[0].addView(ui.text("CAPA E LETRAS SINTÉTICAS · SEM STREAMING",11,Ui.MUTED));
                cover[0]=new ImageView(activity);cover[0].setScaleType(ImageView.ScaleType.FIT_CENTER);root[0].addView(cover[0],new LinearLayout.LayoutParams(-1,ui.dp(170)));Artwork art=new Artwork(activity);art.load(cover[0],HalloweenTests.art());
                seek[0]=new SeekBar(activity);ui.seek(seek[0],playing::get);seek[0].setMax(100);seek[0].setProgress(62);root[0].addView(seek[0],new LinearLayout.LayoutParams(-1,ui.dp(48)));
                toggle[0]=ui.icon("play","Reproduzir",()->{});ui.halloweenTransport(toggle[0]);root[0].addView(toggle[0],new LinearLayout.LayoutParams(ui.dp(64),ui.dp(64)));
                FrameLayout stage=new FrameLayout(activity);scene[0]=new LyricBackdrop(activity,mode::get,playing::get);stage.addView(scene[0],new FrameLayout.LayoutParams(-1,-1));LinearLayout words=ui.column();ui.pad(words,16,14);words.addView(ui.title("Luzes sobre versos sintéticos",21));words.addView(ui.text("Tradução de demonstração",14,Ui.MUTED));stage.addView(words);root[0].addView(stage,new LinearLayout.LayoutParams(-1,ui.dp(230)));activity.setContentView(root[0]);});
            Thread.sleep(650);
            test.runOnMainSync(()->{require(seek[0].getThumb() instanceof SeasonalDrawables.Star,"Christmas seek needs a star thumb");Drawable clipped=((LayerDrawable)seek[0].getProgressDrawable()).getDrawable(1);require(((ClipDrawable)clipped).getDrawable() instanceof SeasonalDrawables.Trail,"Christmas seek needs a light wire");new Ui(activity).setTransport(toggle[0],true);});Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)toggle[0].getDrawable();require(glyph.playing&&glyph.mix==1,"Christmas play-to-pause morph");new Ui(activity).setTransport(toggle[0],false);});Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)toggle[0].getDrawable();require(!glyph.playing&&glyph.mix==0,"Christmas pause-to-play morph");});
            Bitmap[] coverFrames={null,null};test.runOnMainSync(()->{SeasonalDrawables.Cover overlay=(SeasonalDrawables.Cover)cover[0].getForeground();overlay.phase=0;cover[0].invalidate();coverFrames[0]=bitmap(cover[0]);overlay.phase=1.2f;cover[0].invalidate();coverFrames[1]=bitmap(cover[0]);require(!coverFrames[0].sameAs(coverFrames[1]),"Christmas cover lights did not change");coverFrames[0].recycle();coverFrames[1].recycle();});
            test.runOnMainSync(()->{try{HalloweenTests.capture(root[0],new File(folder,"natal.png"));}catch(Exception e){throw new RuntimeException(e);}});
            for(String name:new String[]{"Natal","Neve","Lareira"}){test.runOnMainSync(()->{mode.set(name);scene[0].invalidate();});Thread.sleep(100);test.runOnMainSync(()->{try{HalloweenTests.capture(root[0],new File(folder,name+".png"));}catch(Exception e){throw new RuntimeException(e);}});}
            test.runOnMainSync(()->{playing.set(false);seek[0].invalidate();});
            return new JSONObject().put("passed",true).put("syntheticArtAndLyrics",true).put("physicalPhone",false).put("liveSpotify",false).put("halloweenMorphBothDirections",true).put("christmasMorphBothDirections",true).put("starThumbAndWire",true).put("animatedCoverLights",true).put("christmasScenes",3).put("collectionPlayContrast",true).toString();
        }finally{test.runOnMainSync(()->{AppTheme.choose(app,previous);activity.finish();});}
    }
}
