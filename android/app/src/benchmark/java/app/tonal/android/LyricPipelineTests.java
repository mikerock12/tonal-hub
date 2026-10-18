package app.tonal.android;
import android.app.Instrumentation;
import org.json.*;
import java.util.concurrent.atomic.*;

final class LyricPipelineTests {
    private static void require(boolean yes,String why){if(!yes)throw new AssertionError(why);}
    static String run(Instrumentation i)throws Exception{
        MusicStore[] store={null};MusicLyrics[] lyrics={null};AtomicInteger first=new AtomicInteger(),calls=new AtomicInteger();AtomicBoolean resume=new AtomicBoolean();
        i.runOnMainSync(()->{store[0]=new MusicStore(i.getTargetContext(),"translation-pipeline-test.json");store[0].data=new JSONObject();JSONObject t=new JSONObject();MusicStore.put(t,"provider","Fixture");MusicStore.put(t,"id","pipeline");MusicStore.put(t,"title","Synthetic pipeline");store[0].select(t);store[0].lines=MusicStore.parseLyrics("First\nPending\nLast");lyrics[0]=new MusicLyrics(store[0],(lang,text)->{calls.incrementAndGet();if(text.equals("First")&&first.incrementAndGet()<3)throw new java.io.IOException("Synthetic transient failure");if(text.equals("Pending")&&!resume.get())throw new WebRequest.Failure(400,"");return "PT "+text;});lyrics[0].translate("auto");});
        await(i,store[0]);i.runOnMainSync(()->{require(!store[0].lines.optJSONObject(0).optString("translation").isEmpty(),"temporary failure not retried");require(store[0].lines.optJSONObject(1).optString("translation").isEmpty(),"failed line falsely completed");require(!store[0].lines.optJSONObject(2).optString("translation").isEmpty(),"one failure stopped remaining verses");resume.set(true);lyrics[0].translate("auto");});await(i,store[0]);int requests=calls.get();int[] version={0};
        i.runOnMainSync(()->{require(!store[0].lines.optJSONObject(1).optString("translation").isEmpty(),"manual resume did not repair missing line");store[0].lines=MusicStore.parseLyrics("First\nPending\nLast");version[0]=store[0].lyricVersion;lyrics[0].translate("auto");});await(i,store[0]);i.runOnMainSync(()->{require(calls.get()==requests,"cached translation requested again");require(store[0].lyricVersion>version[0]&&!store[0].lines.optJSONObject(2).optString("translation").isEmpty(),"cached tail not published to UI");lyrics[0].close();});
        return new JSONObject().put("passed",true).put("checks",new JSONArray().put("transient_failure_retries_bounded").put("failed_verse_does_not_stop_remaining_translation").put("manual_resume_translates_only_missing_rows").put("cached_tail_increments_ui_version_without_network")).toString();
    }
    private static void await(Instrumentation i,MusicStore s)throws Exception{long until=System.currentTimeMillis()+22000;AtomicBoolean busy=new AtomicBoolean(true);while(System.currentTimeMillis()<until){i.runOnMainSync(()->busy.set(s.busy));if(!busy.get())return;Thread.sleep(100);}throw new AssertionError("translation pipeline deadline");}
}
