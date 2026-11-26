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
        test.runOnMainSync(()->{store.setPlaybackPriority(true);store.pulse(false,0);require(!store.stories().state().optBoolean("playbackPrioritized"),"rule summary has no playback dependency");store.setPlaybackPriority(false);});
        JSONArray checks=new JSONArray().put("rule_summary_has_no_playback_dependency").put("durable_10005_sessions").put("six_periods_and_year").put("WAL_pinned_archive_query").put("async_statistics").put("full_backup_size_and_restore");
        JSONObject homeTrack=new JSONObject(track.toString()).put("albumId","AAAAAAAAAAAAAAAAAAAAAA");JSONObject homeData=new JSONObject().put("tracks",new JSONObject().put(key,homeTrack)).put("events",events);
        JSONArray deck=HomeAlbums.collect(homeData);require(deck.length()==1&&deck.getJSONObject(0).optLong("listenedMs")==10005L*30000&&deck.getJSONObject(0).optLong("lastAt")==now,"home groups actual album listens and last date");
        homeData.put("events",new JSONArray().put(new JSONObject().put("key",key).put("at",now).put("ms",0)));require(HomeAlbums.collect(homeData).length()==0,"opening an album is not a listen");checks.put("home_album_evidence_and_recency");
        JSONArray identities=new JSONArray().put(track);
        JSONObject jam=new JSONObject().put("session_id","synthetic-session").put("join_session_token","synthetic-token").put("session_members",new JSONArray().put(new JSONObject().put("id","synthetic-member").put("display_name","Pessoa sintética")));
        require(JamClient.parse(jam).optString("link").equals("https://open.spotify.com/socialsession/synthetic-token"),"canonical Jam invitation");jam.put("session_id",123456);require(JamClient.parse(jam)==null,"Jam IDs must be strings");checks.put("jam_typed_session_and_invitation");
        if(nativeModel){
            File dir=new File(app.getFilesDir(),"stories-model");dir.mkdirs();File weights=new File(dir,"Qwen3.5-2B-Q4_K_M.gguf");java.nio.file.Files.write(weights.toPath(),new byte[]{1,2,3});
            LocalStories engine=new LocalStories(app,"native-test");require(!weights.exists(),"legacy Qwen weights must be deleted");require(engine.state().optBoolean("ready")&&!engine.state().optBoolean("busy"),"rule summary is immediately ready");
            track.put("genre","Grunge").put("tone",new JSONObject().put("intenso",4));engine.analyze(new JSONArray().put(track),0);String summary=engine.state().optString("summary");require(summary.contains("Grunge")&&summary.contains("intenso")&&summary.contains("sem modelo"),"deterministic genre and mood summary");
            checks.put("deterministic_genre_mood_summary").put("legacy_Qwen_weights_removed");
        }
        db.close();return new JSONObject().put("checks",checks).put("input","Synthetic history and original Portuguese descriptions; no live Spotify or physical phone").toString();
    }
}
