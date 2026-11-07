package app.tonal.android;

import android.content.Context;
import java.util.ArrayList;
import java.util.Collections;
import org.json.JSONObject;

/** Real public MusicBrainz lookup and the shared planner, for an artist Spotify gives no genre. */
final class DiscoveryStyleTests {
    private static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    private static JSONObject track(String id,String title,String artist,String album){
        JSONObject t=new JSONObject();MusicStore.put(t,"provider","Spotify");MusicStore.put(t,"id","spotify:track:"+id);MusicStore.put(t,"uri","spotify:track:"+id);MusicStore.put(t,"title",title);MusicStore.put(t,"artist",artist);MusicStore.put(t,"album",album);MusicStore.put(t,"artistId","");return t;
    }
    private static void row(ArrayList<String> out,String id,String artist,String genre){Collections.addAll(out,id,artist,"album-"+id,genre,"1994","0","0");}
    static String run(Context context)throws Exception{
        MusicStore store=MusicStore.get(context);
        JSONObject anchor=track("0000000000000000000011","Sad Tomorrow","The Muffs","Blonder and Blonder");
        JSONObject checks=MusicStore.object(store.data,"metadataChecks");checks.remove(MusicStore.key(anchor));MusicStore.object(store.data,"tracks").remove(MusicStore.key(anchor));MusicStore.object(store.data,"publicArtistMetadata").remove("The Muffs|Blonder and Blonder");long started=android.os.SystemClock.elapsedRealtime();
        store.remember(anchor);MusicMetadata metadata=new MusicMetadata(store,null);
        try{
            require(metadata.priority(anchor),"an unchecked track must be looked up");
            String genre="";long until=android.os.SystemClock.elapsedRealtime()+20000;
            while(genre.isEmpty()&&android.os.SystemClock.elapsedRealtime()<until){Thread.sleep(250);genre=MusicStore.object(MusicStore.object(store.data,"tracks"),MusicStore.key(anchor)).optString("genre");}
            if(genre.isEmpty()){String detail;try{JSONObject found=WebRequest.musicBrainz("artist/?fmt=json&limit=8&query="+SpotifyClient.encode("artist:\"The Muffs\""));JSONObject unique=MusicMetadata.uniqueArtist(found,"The Muffs");detail="artists="+MusicStore.array(found,"artists").length()+" unique="+(unique!=null);if(unique!=null){JSONObject full=WebRequest.musicBrainz("artist/"+unique.optString("id")+"?fmt=json&inc=genres+tags");detail+=" genres="+MusicMetadata.genres(full)+" raw="+full.toString().substring(0,Math.min(200,full.toString().length()));}}catch(Exception e){detail=e.toString();}throw new AssertionError("MusicBrainz gave no verified style: "+detail+" check="+MusicStore.object(MusicStore.object(store.data,"metadataChecks"),MusicStore.key(anchor)));}
            long looked=android.os.SystemClock.elapsedRealtime()-started;
            String tags=DiscoveryStyles.merge(genre,new ArrayList<>());String first=tags.split(",")[0];
            String[] queries=ListeningMixNative.queries(tags,1994);require(queries.length>0&&queries[0].startsWith("genre:\""),"styles must become catalog queries");
            ArrayList<String> source=new ArrayList<>(),pool=new ArrayList<>();
            for(int i=0;i<4;i++)row(source,"s"+i,"The Muffs",i==0?genre:"");row(pool,"p0","Other Band",first);row(pool,"p1","Another Band",first);
            int[] plan=ListeningMixNative.plan(source.toArray(new String[0]),pool.toArray(new String[0]),0,false,true,1234,false);
            int outside=0;for(int i:plan)if(i<0)outside++;
            require(outside>0,"a candidate sharing the style must enter the queue");
            return new JSONObject().put("passed",true).put("lookupMs",looked).put("genre",genre).put("queries",queries.length).put("outside",outside).put("realMusicBrainz",true).put("liveSpotify",false).toString();
        }finally{metadata.close();}
    }
}
