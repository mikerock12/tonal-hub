package app.tonal.android;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import java.util.concurrent.atomic.AtomicBoolean;
import org.json.JSONArray;
import org.json.JSONObject;

/** Decorations across real AudioTrack transitions: a skip must not read as an audio failure. */
final class TrackChangeTests {
    private static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    private static final int SKIPS=4;
    static String run(Instrumentation test)throws Exception{
        Context app=test.getTargetContext();String previous=AppTheme.chosen(app);
        Activity activity=test.startActivitySync(new Intent(app,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        AudioOutput audio=NativeBridge.audio;audio.initialize(app);audio.open(44100);
        AtomicBoolean playing=new AtomicBoolean(true),writing=new AtomicBoolean(true),feed=new AtomicBoolean(true);
        Thread writer=new Thread(()->{
            float[] pcm=new float[2205*2];long n=0;
            while(writing.get()){
                if(!feed.get()){try{Thread.sleep(5);}catch(InterruptedException e){return;}continue;}
                for(int i=0;i<2205;i++,n++)pcm[i*2]=pcm[i*2+1]=(float)(.02*Math.sin(2*Math.PI*440*n/44100));
                audio.write(pcm);
            }
        },"TrackChangeTone");writer.start();
        ChristmasStage[] stage={null};
        try{
            test.runOnMainSync(()->AppTheme.choose(app,"natal"));
            test.runOnMainSync(()->{ImageView cover=new ImageView(activity);cover.setScaleType(ImageView.ScaleType.FIT_CENTER);FrameLayout root=new FrameLayout(activity);stage[0]=new ChristmasStage(activity,cover,playing::get,0);root.addView(stage[0],new FrameLayout.LayoutParams(-1,600));activity.setContentView(root);new Artwork(activity).load(cover,HalloweenTests.art());});
            for(int wait=0;wait<80&&stage[0].santa==null;wait++)Thread.sleep(100);
            Thread.sleep(2600);android.graphics.Rect area=new android.graphics.Rect();boolean[] shown={false};test.runOnMainSync(()->shown[0]=SeasonalDrawables.visible(stage[0],area));require(stage[0].running,"decorations run before the first skip: visible="+shown[0]+" allowed="+ChristmasLights.allowed(app)+" underruns="+audio.underruns()+" focus="+stage[0].hasWindowFocus()+" animators="+android.animation.ValueAnimator.areAnimatorsEnabled()+" area="+area);
            int before=audio.underruns();JSONArray seen=new JSONArray();
            for(int skip=0;skip<SKIPS;skip++){
                // Same order as a skip: producer stops, sink closes, the next track loads, sink starts.
                feed.set(false);audio.pause();playing.set(false);audio.discontinuity();Thread.sleep(700);
                audio.startStereo(44100);playing.set(true);Thread.sleep(150);feed.set(true);
                Thread.sleep(3200);
                seen.put(new JSONObject().put("skip",skip+1).put("underruns",audio.underruns()-before).put("running",stage[0].running).put("santa",stage[0].santa.isRunning()));
            }
            int skipped=audio.underruns()-before;
            // Repeated gaps in steady playback are the case the guard exists for.
            Thread.sleep(8500);boolean quieted=false;
            for(int stall=0;stall<6&&!quieted;stall++){feed.set(false);Thread.sleep(450);feed.set(true);Thread.sleep(2300);quieted=!stage[0].running;}
            require(quieted,"repeated steady underruns must still silence the decorations");
            Thread.sleep(16000);
            boolean alive=stage[0].running&&stage[0].santa.isRunning();
            JSONObject result=new JSONObject().put("passed",alive).put("underrunsFromSkips",skipped).put("steadyUnderrunsSilence",quieted).put("runs",seen).put("realAudioTrack",true).put("liveSpotify",false);
            require(alive,"decorations stopped after a track change: "+result);
            return result.toString();
        }finally{writing.set(false);feed.set(true);writer.join(1000);test.runOnMainSync(()->{AppTheme.choose(app,previous);activity.finish();});}
    }
}
