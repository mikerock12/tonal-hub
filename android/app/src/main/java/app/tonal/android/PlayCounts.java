package app.tonal.android;

import android.os.Handler;
import android.os.Looper;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.*;
import java.util.function.Consumer;

/** Public Spotify play counts and artist overviews through the native session (0.18.1).
 * One catalog request at a time, never through the Jam slot; bounded caches, main thread only. */
final class PlayCounts {
    private static final int MAX_TRACKS=6000,MAX_ALBUMS=1500,BATCH=8;
    private static final long TTL=6L*3600000,RETRY_MS=60000;
    private static PlayCounts instance;
    static PlayCounts get(){if(instance==null)instance=new PlayCounts();return instance;}

    private final Handler main=new Handler(Looper.getMainLooper());
    private final LinkedHashMap<String,Long> plays=new LinkedHashMap<>(256,.75f,true){
        @Override protected boolean removeEldestEntry(Map.Entry<String,Long> e){return size()>MAX_TRACKS;}};
    private final LinkedHashMap<String,Long> albumsDone=new LinkedHashMap<>(64,.75f,true){
        @Override protected boolean removeEldestEntry(Map.Entry<String,Long> e){return size()>MAX_ALBUMS;}};
    private final ArrayDeque<String> albumQueue=new ArrayDeque<>();
    private final HashSet<String> queued=new HashSet<>();
    private final ArrayDeque<String> artistQueue=new ArrayDeque<>();
    private final HashMap<String,Consumer<JSONObject>> artistWaiting=new HashMap<>();
    private final HashMap<String,JSONObject> artistCache=new HashMap<>();
    private final HashMap<String,Long> artistAt=new HashMap<>();
    private final ArrayList<Runnable> listeners=new ArrayList<>();
    private final ArrayList<String> inFlight=new ArrayList<>();
    private int nextRequest=1_000_000_000,pending=-1;String pendingOp="",pendingArtist="";
    private long blockedUntil;

    private PlayCounts(){NativeBridge.artistStatsListener=this::result;}

    /** Known public plays of one track, or -1. Never estimated. */
    long plays(String uri){Long value=plays.get(uri);return value==null?-1:value;}
    void listen(Runnable listener){if(!listeners.contains(listener))listeners.add(listener);}
    void forget(Runnable listener){listeners.remove(listener);}
    /** Albums whose counts are known or definitively answered in this period. */
    boolean albumKnown(String id){Long at=albumsDone.get(id);return at!=null&&System.currentTimeMillis()-at<TTL;}
    int pendingAlbums(){return albumQueue.size()+(pendingOp.equals("albumPlays")?inFlight.size():0);}

    /** Queue album ids (22 chars) for their per-track plays. Duplicates and fresh albums are skipped. */
    void requestAlbums(Collection<String> ids){
        boolean added=false;
        for(String id:ids){if(id==null||!id.matches("[A-Za-z0-9]{22}")||albumKnown(id)||queued.contains(id))continue;queued.add(id);albumQueue.add(id);added=true;}
        if(added)pump();
    }
    static Set<String> albumIds(Collection<JSONObject> tracks){LinkedHashSet<String> ids=new LinkedHashSet<>();for(JSONObject t:tracks){JSONObject album=t.optJSONObject("album");String id=album!=null?album.optString("id"):t.optString("albumId");if(!id.isEmpty())ids.add(id);}return ids;}

    void artist(String id,Consumer<JSONObject> ready){
        if(!id.matches("[A-Za-z0-9]{22}"))return;
        JSONObject cached=artistCache.get(id);Long at=artistAt.get(id);
        if(cached!=null&&at!=null&&System.currentTimeMillis()-at<TTL){ready.accept(cached);return;}
        artistWaiting.put(id,ready);if(!artistQueue.contains(id)&&!(pendingOp.equals("artistStats")&&pendingArtist.equals(id)))artistQueue.addFirst(id);pump();
    }
    void cancelArtist(String id){artistWaiting.remove(id);artistQueue.remove(id);}

    private void pump(){
        if(pending>=0||!NativeBridge.ready)return;
        long now=System.currentTimeMillis();if(now<blockedUntil){main.postDelayed(this::pump,blockedUntil-now);return;}
        JSONObject command=new JSONObject();int id=nextRequest++;if(nextRequest>2_000_000_000)nextRequest=1_000_000_000;
        if(!artistQueue.isEmpty()){
            pendingArtist=artistQueue.poll();pendingOp="artistStats";
            MusicStore.put(command,"op","artistStats");MusicStore.put(command,"uri","spotify:artist:"+pendingArtist);
        }else if(!albumQueue.isEmpty()){
            inFlight.clear();JSONArray albums=new JSONArray();
            while(inFlight.size()<BATCH&&!albumQueue.isEmpty()){String album=albumQueue.poll();inFlight.add(album);albums.put("spotify:album:"+album);}
            pendingOp="albumPlays";MusicStore.put(command,"op","albumPlays");MusicStore.put(command,"albums",albums);
        }else return;
        MusicStore.put(command,"request",id);pending=id;
        final int sent=id;main.postDelayed(()->{if(pending==sent)finish(null);},40000);
        NativeBridge.jamRequest(command.toString());
    }

    private void result(String text){
        try{JSONObject response=new JSONObject(text);if(response.optInt("request",-1)!=pending||pending<0)return;finish(response);}catch(Exception ignored){}
    }

    private void finish(JSONObject response){
        int code=response==null?408:response.optInt("status");JSONObject result=response==null?null:response.optJSONObject("result");
        String op=pendingOp;pending=-1;pendingOp="";long now=System.currentTimeMillis();
        if(code==409){if(op.equals("artistStats"))artistQueue.addFirst(pendingArtist);else{for(int i=inFlight.size()-1;i>=0;i--)albumQueue.addFirst(inFlight.get(i));}inFlight.clear();main.postDelayed(this::pump,1500);return;}
        if(op.equals("artistStats")){
            Consumer<JSONObject> ready=artistWaiting.remove(pendingArtist);
            if(code/100==2&&result!=null){artistCache.put(pendingArtist,result);artistAt.put(pendingArtist,now);while(artistCache.size()>48){String old=artistCache.keySet().iterator().next();artistCache.remove(old);artistAt.remove(old);}
                JSONObject counts=MusicStore.object(result,"playCounts");boolean changed=false;for(Iterator<String> keys=counts.keys();keys.hasNext();){String uri=keys.next();plays.put(uri,counts.optLong(uri));changed=true;}
                if(ready!=null)ready.accept(result);if(changed)notifyListeners();}
            else if(ready!=null)ready.accept(null);
            if(code==429||code==503)blockedUntil=now+RETRY_MS;
        }else{
            HashSet<String> done=new HashSet<>();
            if(code/100==2&&result!=null){JSONObject counts=MusicStore.object(result,"plays");for(Iterator<String> keys=counts.keys();keys.hasNext();){String uri=keys.next();plays.put(uri,counts.optLong(uri));}
                for(Object v:MusicStore.values(MusicStore.array(result,"albums"))){String uri=String.valueOf(v);if(uri.startsWith("spotify:album:"))done.add(uri.substring(14));}}
            for(String album:inFlight){queued.remove(album);if(done.contains(album))albumsDone.put(album,now);}
            // Failed albums stay unknown; they are queued again only by a later screen request.
            inFlight.clear();if(code==429||code==503)blockedUntil=now+RETRY_MS;
            notifyListeners();
        }
        pump();
    }
    private void notifyListeners(){for(Runnable r:new ArrayList<>(listeners))r.run();}

    static String compact(long value){return CatalogFormat.compact(value);}
}
