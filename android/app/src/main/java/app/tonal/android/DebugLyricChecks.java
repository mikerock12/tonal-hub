package app.tonal.android;
import android.os.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicInteger;

/** Synthetic outage regression, reachable only through the debuggable test fixture. */
final class DebugLyricChecks {
    static void run(MainActivity host,MusicStore store,Runnable done){
        if(!BuildConfig.DEBUG)return;
        AtomicInteger calls=new AtomicInteger(),searches=new AtomicInteger();
        MusicLyrics client=new MusicLyrics(store,null,url->{calls.incrementAndGet();if(url.contains("/api/get"))throw new WebRequest.Failure(503,"");searches.incrementAndGet();return new JSONArray().put(new JSONObject().put("trackName",store.current.optString("title")).put("artistName",store.current.optString("artist")).put("duration",180).put("syncedLyrics","[00:01.00]Synthetic first verse\n[00:03.00]Synthetic second verse\n[00:05.00]Synthetic last verse"));});
        client.fetch();Handler handler=new Handler(Looper.getMainLooper());long deadline=SystemClock.elapsedRealtime()+20000;
        handler.postDelayed(new Runnable(){public void run(){if(store.busy&&SystemClock.elapsedRealtime()<deadline){handler.postDelayed(this,100);return;}try{JSONObject report=new JSONObject().put("passed",!store.busy&&store.lines.length()==3&&calls.get()==4&&searches.get()==1).put("requests",calls.get()).put("search_fallback",searches.get()).put("busy_cleared",!store.busy).put("lines",store.lines.length());java.nio.file.Files.write(new File(host.getExternalFilesDir(null),"lyrics-recovery-tests.json").toPath(),report.toString().getBytes(StandardCharsets.UTF_8));client.close();done.run();}catch(Exception e){throw new IllegalStateException(e);}}},100);
    }
}
