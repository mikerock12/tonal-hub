package app.tonal.android;
import android.app.Instrumentation;
import android.content.Context;
import android.net.Uri;
import org.json.*;
import java.io.File;
import java.util.concurrent.*;

/** Only synthetic data, only the separate benchmark application. */
final class HistoryStoryTests {
    private static void require(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static String run(Instrumentation test,boolean nativeModel)throws Exception{
        Context app=test.getTargetContext();String name="history-story-0150.sqlite";app.deleteDatabase(name);
        JSONObject track=new JSONObject().put("provider","Spotify").put("id","synthetic-history").put("title","Canção de demonstração").put("artist","Artista sintético").put("album","Álbum sintético").put("durationMs",180000);String key=MusicStore.key(track);
        JSONArray events=new JSONArray();long now=System.currentTimeMillis();for(int i=0;i<10005;i++)events.put(new JSONObject().put("session",String.valueOf(i)).put("key",key).put("at",now-(10004L-i)*3600000).put("ms",30000).put("play",true));
        PersonalDatabase db=new PersonalDatabase(app,name);JSONObject original=new JSONObject().put("tracks",new JSONObject().put(key,track)).put("events",events);db.write(original.toString());JSONObject recent=db.read();require(MusicStore.array(recent,"events").length()==10000,"bounded recent cache");db.write(recent.toString());require(MusicStore.array(db.fullSnapshot(),"events").length()==10005,"durable archive after cache eviction");
        JSONObject snapshot=db.historySnapshot(recent,0);MusicStore store=new MusicStore(app,"history-story-0150.json");JSONObject all=MusicInsights.report(store,0,"artists",30,snapshot),year=MusicInsights.report(store,365,"artists",30,snapshot);require(all.optInt("plays")==10005&&all.optLong("ms")==10005L*30000,"archive totals without duplicates");require(year.optInt("plays")>8000&&year.optInt("plays")<9000,"year cutoff");
        for(int days:new int[]{7,30,90,180,365,0})require(MusicInsights.report(store,days,"artists",30,snapshot).optInt("plays")>0,"period "+days);
        CountDownLatch reportReady=new CountDownLatch(1);test.runOnMainSync(()->store.requestStatistics(0,"artists",reportReady::countDown));require(reportReady.await(25,TimeUnit.SECONDS)&&store.statisticsReport.optInt("plays")==10005,"asynchronous full report");
        File backup=new File(app.getCacheDir(),"history-story-0150.json");CountDownLatch exported=new CountDownLatch(1);boolean[] ok={false};test.runOnMainSync(()->store.exportFullBackup(Uri.fromFile(backup),success->{ok[0]=success;exported.countDown();}));require(exported.await(25,TimeUnit.SECONDS)&&ok[0]&&backup.length()==store.data.optLong("lastBackupBytes"),"full backup and byte size");
        test.runOnMainSync(store::clearHistory);CountDownLatch restored=new CountDownLatch(1);test.runOnMainSync(()->store.restoreFullBackup(Uri.fromFile(backup),success->{ok[0]=success;restored.countDown();}));require(restored.await(25,TimeUnit.SECONDS)&&ok[0],"full backup restore");
        test.runOnMainSync(()->{store.setPlaybackPriority(true);store.pulse(false,0);require(store.stories().state().optBoolean("playbackPrioritized"),"buffering must preserve playback priority");store.setPlaybackPriority(false);require(!store.stories().state().optBoolean("playbackPrioritized"),"explicit pause releases priority");});
        JSONArray checks=new JSONArray().put("buffering_priority_and_explicit_pause").put("durable_10005_sessions").put("six_periods_and_year").put("WAL_pinned_archive_query").put("async_statistics").put("full_backup_size_and_restore");
        if(nativeModel){
            // The production reader is requested from a visible Activity, not a background test UID.
            android.app.Activity screen=test.startActivitySync(new android.content.Intent(app,MainActivity.class).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK));
            LocalStories engine=new LocalStories(app,"native-test");long readyDeadline=android.os.SystemClock.elapsedRealtime()+5000;while(!engine.state().optBoolean("ready")&&android.os.SystemClock.elapsedRealtime()<readyDeadline)Thread.sleep(50);require(engine.state().optBoolean("ready"),"verified Qwen fixture not installed");
            engine.capture(track,"Uma canção sobre alguém que volta à casa da infância e reencontra lembranças de uma amizade distante. A distância causa tristeza, mas a pessoa segue em frente e encontra esperança no reencontro.");
            CountDownLatch generated=new CountDownLatch(1);test.runOnMainSync(()->{engine.observe(()->{JSONObject state=engine.state();if(!state.optString("summary").isEmpty())generated.countDown();});engine.analyze(new JSONArray().put(track),0);});
            require(generated.await(110,TimeUnit.SECONDS),"native summary deadline: "+engine.state().optString("status"));JSONObject result=engine.state();require(result.optString("summary").length()>20,"empty native narrative");test.runOnMainSync(engine::detach);require(!engine.state().optBoolean("busy"),"cancelled reader still busy");checks.put("real_Qwen_JNI_service_inference").put("bound_service_released");test.runOnMainSync(screen::finish);
        }
        db.close();return new JSONObject().put("checks",checks).put("input","Synthetic history and original Portuguese descriptions; no live Spotify or physical phone").toString();
    }
}
