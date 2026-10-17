package app.tonal.android;
import android.content.Context;
import org.json.*;

final class CompanionDeviceTests {
    private static void require(boolean pass,String label){if(!pass)throw new AssertionError(label);}
    static JSONArray run(Context context)throws Exception{
        JSONArray checks=new JSONArray();
        JSONArray lrc=MusicStore.parseLyrics("[offset:-100]\n[ar:Synthetic]\n[00:01.20][00:02.345]Test\n[00:00.50]Earlier");
        require(lrc.length()==3&&lrc.getJSONObject(0).getInt("time")==400&&lrc.getJSONObject(2).getInt("time")==2245,"LRC parser");checks.put("companion_lrc_multistamps_offset_and_fractions");
        MusicStore store=new MusicStore(context,"companion-test-library.json");store.data=new JSONObject();JSONObject t=new JSONObject().put("provider","Internet Archive").put("id","fixture/01.mp3").put("title","Synthetic Fire").put("artist","Fixture").put("genre","punk").put("playable",true).put("stream","https://example.org/audio?token=do-not-persist").put("password","do-not-persist");
        require(!MusicStore.safe(t).has("stream")&&!MusicStore.safe(t).has("password"),"private fields persisted");checks.put("companion_signed_urls_and_passwords_excluded");
        store.select(t);store.favorite(t,true);String id=store.createPlaylist("Synthetic playlist");store.add(id,t);store.add(id,t);require(store.favorites().length()==1&&MusicStore.array(MusicStore.array(store.data,"playlists").getJSONObject(0),"tracks").length()==1,"playlist/favorite dedup");checks.put("companion_favorites_and_playlist_deduplication");
        store.lines=MusicStore.parseLyrics("fire rage fight\ndance");store.analyze();JSONArray events=new JSONArray();for(int i=0;i<3;i++)events.put(new JSONObject().put("key",MusicStore.key(t)).put("at",System.currentTimeMillis()-1000-i*60000).put("ms",60000).put("play",true));events.put(new JSONObject().put("key",MusicStore.key(t)).put("at",System.currentTimeMillis()-20L*86400000).put("ms",120000).put("play",true));MusicStore.put(store.data,"events",events);
        JSONObject week=store.statistics(7,"artists"),month=store.statistics(30,"genres");require(week.getLong("ms")==180000&&week.getInt("plays")==3&&month.getLong("ms")==300000,"period totals");require(week.getString("vibe").equals("Clima intenso")&&week.getJSONArray("recommendations").length()>0,"lyric and genre evidence");checks.put("companion_periods_chart_drilldown_lyric_mood_and_genre_suggestions");
        store.clearHistory();require(store.statistics(7,"artists").getInt("plays")==0&&store.favorites().length()==1,"clear history damaged favorites");checks.put("companion_clear_history_preserves_library");
        JSONObject live=new JSONObject().put("identifier","ajc02607_l7-1989-09-14").put("mediatype","audio").put("title","L7 Live at Dreamerz 1989-09-14");require(IndieCatalog.recordingKind(live).equals("show")&&IndieCatalog.recordingKind(new JSONObject().put("title","L7 Demos")).equals("demo"),"rare audio classification");
        IndieCatalog.Item route=new IndieCatalog.Item("Internet Archive","fixture","show","Synthetic show");route.artist="L7";IndieCatalog.Batch b=new IndieCatalog.Batch("Internet Archive");JSONObject meta=new JSONObject().put("metadata",new JSONObject().put("title","Fixture"));JSONArray files=new JSONArray().put(new JSONObject().put("name","01.flac").put("format","24bit Flac")).put(new JSONObject().put("name","01.mp3").put("original","01.flac").put("format","VBR MP3")).put(new JSONObject().put("name","02.mp3").put("format","MP3").put("private","true")).put(new JSONObject().put("name","checksums.ffp").put("format","Flac FingerPrint")).put(new JSONObject().put("name","../escape.mp3").put("format","MP3"));meta.put("files",files);IndieCatalog.parseArchiveFiles(b,meta,route);require(b.items.size()==1&&b.items.get(0).id.equals("fixture/01.mp3"),"public audio variants");checks.put("rare_archive_audio_demos_mp3_preference_private_and_path_filter");
        boolean rejected=false;try{meta.put("is_dark",true);IndieCatalog.parseArchiveFiles(new IndieCatalog.Batch("Internet Archive"),meta,route);}catch(Exception expected){rejected=true;}require(rejected,"dark audio accepted");checks.put("rare_archive_dark_items_rejected");
        String backup=store.exportBackup();store.delete(id);store.restoreBackup(backup);require(MusicStore.array(store.data,"playlists").length()==1,"portable backup restore");checks.put("companion_portable_backup_restore");
        java.nio.file.Files.write(new java.io.File(context.getFilesDir(),"companion-test-backup.json").toPath(),backup.getBytes(java.nio.charset.StandardCharsets.UTF_8));
        store.flush();java.lang.reflect.Field disk=MusicStore.class.getDeclaredField("disk");disk.setAccessible(true);((java.util.concurrent.ExecutorService)disk.get(store)).submit(()->{}).get(10,java.util.concurrent.TimeUnit.SECONDS);
        MusicStore restored=new MusicStore(context,"companion-test-library.json");require(restored.favorites().length()==1&&MusicStore.array(restored.data,"playlists").length()==1,"SQLite restart persistence");checks.put("companion_sqlite_transaction_persistence");
        return checks;
    }
}
