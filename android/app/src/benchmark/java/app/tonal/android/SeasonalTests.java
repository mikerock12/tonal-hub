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
            LinearLayout[] root={null};ImageView[] cover={null};ChristmasStage[] stage3={null};SeekBar[] seek={null};ImageButton[] toggle={null};LyricBackdrop[] scene={null};
            test.runOnMainSync(()->{Ui ui=new Ui(activity);root[0]=ui.column();root[0].setBackgroundColor(Ui.BG);ui.pad(root[0],18,12);root[0].addView(ui.title("Natal · componentes reais",21));root[0].addView(ui.text("CAPA E LETRAS SINTÉTICAS · SEM STREAMING",11,Ui.MUTED));
                cover[0]=new ImageView(activity);cover[0].setScaleType(ImageView.ScaleType.FIT_CENTER);stage3[0]=new ChristmasStage(activity,cover[0],playing::get,ui.dp(14));root[0].addView(stage3[0],new LinearLayout.LayoutParams(-1,ui.dp(280)));Artwork art=new Artwork(activity);art.load(cover[0],HalloweenTests.art());
                seek[0]=new SeekBar(activity);ui.seek(seek[0],playing::get);seek[0].setMax(100);seek[0].setProgress(62);root[0].addView(seek[0],new LinearLayout.LayoutParams(-1,ui.dp(68)));
                toggle[0]=ui.icon("play","Reproduzir",()->{});ui.halloweenTransport(toggle[0]);root[0].addView(toggle[0],new LinearLayout.LayoutParams(ui.dp(64),ui.dp(64)));
                FrameLayout stage=new FrameLayout(activity);scene[0]=new LyricBackdrop(activity,mode::get,playing::get);stage.addView(scene[0],new FrameLayout.LayoutParams(-1,-1));LinearLayout words=ui.column();ui.pad(words,16,14);words.addView(ui.title("Luzes sobre versos sintéticos",21));words.addView(ui.text("Tradução de demonstração",14,Ui.MUTED));stage.addView(words);root[0].addView(stage,new LinearLayout.LayoutParams(-1,ui.dp(160)));activity.setContentView(root[0]);});
            Thread.sleep(650);
            test.runOnMainSync(()->{require(seek[0].getThumb() instanceof SeasonalDrawables.Star,"Christmas seek needs a star thumb");require(((LayerDrawable)seek[0].getProgressDrawable()).getDrawable(1) instanceof SeasonalDrawables.Trail,"Christmas seek needs a light wire");new Ui(activity).setTransport(toggle[0],true);});Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)toggle[0].getDrawable();require(glyph.playing&&glyph.mix==1,"Christmas play-to-pause morph");new Ui(activity).setTransport(toggle[0],false);});Thread.sleep(420);
            test.runOnMainSync(()->{SeasonalDrawables.TransportGlyph glyph=(SeasonalDrawables.TransportGlyph)toggle[0].getDrawable();require(!glyph.playing&&glyph.mix==0,"Christmas pause-to-play morph");});
            for(int wait=0;wait<80&&(stage3[0].santa==null||ChristmasAssets.get(app,null)==null);wait++)Thread.sleep(100);
            test.runOnMainSync(()->{require(ChristmasAssets.get(app,null).bulbs.length>=50,"Garland needs many bulbs");require(cover[0].getForeground()==null,"Hero cover must not also carry the still garland");
                Bitmap[] frames={null,null};stage3[0].phase=0;frames[0]=bitmap(stage3[0]);stage3[0].phase=1.2f;frames[1]=bitmap(stage3[0]);require(!frames[0].sameAs(frames[1]),"Christmas cover lights did not change");
                int garland=0;for(int y=0;y<frames[0].getHeight();y+=4)if(Color.alpha(frames[0].getPixel((int)(stage3[0].art.left-stage3[0].art.width()*.03f),y))>0)garland++;require(garland>0,"Back layer must show beside the cover");frames[0].recycle();frames[1].recycle();
                SeasonalDrawables.Trail trail=(SeasonalDrawables.Trail)((LayerDrawable)seek[0].getProgressDrawable()).getDrawable(1);
                seek[0].setProgress(0);require(trail.litBulbs(10)==0,"No bulb is lit before the star passes");seek[0].setProgress(50);require(trail.litBulbs(10)==5,"Bulbs behind the star are lit, the rest stay dark");seek[0].setProgress(100);require(trail.litBulbs(10)==10,"All bulbs lit at the end");seek[0].setProgress(12);require(trail.litBulbs(10)==1,"Seeking back switches bulbs off");seek[0].setProgress(62);});
            if(android.animation.ValueAnimator.areAnimatorsEnabled()){
                Thread.sleep(800);require(stage3[0].running&&stage3[0].santa.isRunning(),"Santa dances while music plays");
                playing.set(false);Thread.sleep(800);require(!stage3[0].running&&!stage3[0].santa.isRunning(),"Santa must stop with the music");
                playing.set(true);Thread.sleep(800);require(stage3[0].santa.isRunning(),"Santa resumes with the music");
            }
            test.runOnMainSync(()->{try{HalloweenTests.capture(root[0],new File(folder,"natal.png"));}catch(Exception e){throw new RuntimeException(e);}});
            for(String name:new String[]{"Natal","Neve","Lareira"}){test.runOnMainSync(()->{mode.set(name);scene[0].invalidate();});Thread.sleep(100);test.runOnMainSync(()->{try{HalloweenTests.capture(root[0],new File(folder,name+".png"));}catch(Exception e){throw new RuntimeException(e);}});}
            test.runOnMainSync(()->{playing.set(false);seek[0].invalidate();});
            return new JSONObject().put("passed",true).put("syntheticArtAndLyrics",true).put("physicalPhone",false).put("liveSpotify",false).put("halloweenMorphBothDirections",true).put("christmasMorphBothDirections",true).put("starThumbAndWire",true).put("animatedCoverLights",true).put("garlandBackAndFront",true).put("seekLightsFollowStar",true).put("santaFollowsPlayback",android.animation.ValueAnimator.areAnimatorsEnabled()).put("christmasScenes",3).put("collectionPlayContrast",true).toString();
        }finally{test.runOnMainSync(()->{AppTheme.choose(app,previous);activity.finish();});}
    }
}
