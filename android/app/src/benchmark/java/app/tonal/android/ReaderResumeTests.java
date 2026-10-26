package app.tonal.android;
import android.app.Instrumentation;
import android.content.Context;
import org.json.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

/** Optional real-weight test in the dedicated AVD; no Spotify, network or user data. */
final class ReaderResumeTests {
    static void require(boolean yes,String why){if(!yes)throw new AssertionError(why);}
    static String run(Instrumentation test)throws Exception {
        Context app=test.getTargetContext();android.app.Activity screen=test.startActivitySync(new android.content.Intent(app,MainActivity.class).putExtra("mixUiFixture",true).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK));
        LocalStories engine=new LocalStories(app,"resume-0152-test");engine.debugBudgetMs=20000;
        try {
        long ready=System.currentTimeMillis()+8000;while(!engine.state().optBoolean("ready")&&System.currentTimeMillis()<ready)Thread.sleep(100);
        require(engine.state().optBoolean("ready"),"verified real model fixture missing");
        JSONObject track=new JSONObject().put("provider","Fixture").put("id","resume-0152").put("title","Versos originais de teste").put("artist","Fixture");
        StringBuilder words=new StringBuilder();for(int n=0;n<30;n++)words.append("On evening ").append(n).append(" I remember our old home, then open the door and choose a different road with hope.\n");
        engine.capture(track,words.toString());JSONArray batch=new JSONArray().put(track);
        test.runOnMainSync(()->engine.analyze(batch,7));await(test,engine);
        JSONObject first=report(app);require(engine.state().optBoolean("partial")&&first.optBoolean("resumable"),"bounded first action must preserve unfinished work even with zero accepted readings");
        test.runOnMainSync(()->engine.analyze(batch,7));await(test,engine);JSONObject second=report(app);
        require(second.optBoolean("resumed"),"second native process restarted instead of restoring its checkpoint");
        int before=first.optInt("progressDone"),after=second.optInt("progressDone");
        require(second.optBoolean("complete")||!first.optString("progressStage").equals(second.optString("progressStage"))||after>before,"resumed process made no forward progress");
        test.runOnMainSync(()->{engine.setPlaying(true);engine.analyze(batch,7);});Thread.sleep(1500);
        require(engine.state().optBoolean("busy")&&engine.state().optString("status").contains("pausa"),"playback priority must defer inference");
        test.runOnMainSync(engine::detach);require(!engine.state().optBoolean("busy"),"closing reader must cancel pending work");
        System.loadLibrary("tonal_story");boolean fast=StoryInferenceService.supportsAcceleratedCpu();
        return new JSONObject().put("passed",true).put("physicalPhone",false).put("realWeights",true).put("firstStage",first.optString("progressStage")).put("secondStage",second.optString("progressStage")).put("firstDone",before).put("secondDone",after).put("resumed",true).put("acceleratedCpuSupported",fast).put("meaningQualityValidated",false).toString();
        } finally {test.runOnMainSync(()->{engine.detach();screen.finish();});}
    }
    private static void await(Instrumentation test,LocalStories engine)throws Exception{long end=System.currentTimeMillis()+40000;boolean[] busy={true};while(System.currentTimeMillis()<end){test.runOnMainSync(()->busy[0]=engine.state().optBoolean("busy"));if(!busy[0])return;Thread.sleep(100);}throw new AssertionError("bounded native reader did not return");}
    private static JSONObject report(Context app)throws Exception{File[] files=new File(app.getCacheDir(),"stories-resume-0152-test/summaries").listFiles(f->f.getName().endsWith(".json"));require(files!=null&&files.length>0,"valid resumable report was not persisted");Arrays.sort(files,Comparator.comparingLong(File::lastModified).reversed());return new JSONObject(new String(Files.readAllBytes(files[0].toPath()),StandardCharsets.UTF_8));}
}
