package app.tonal.android;

import org.json.*;
import java.util.*;

/** Local listening evidence only: merely opening an album does not make it recent. */
final class HomeAlbums {
    static JSONArray collect(JSONObject data) throws JSONException {
        JSONObject tracks=MusicStore.object(data,"tracks");
        Map<String,JSONObject> albums=new HashMap<>();
        long now=System.currentTimeMillis();
        for(Object value:MusicStore.values(MusicStore.array(data,"events"))) {
            JSONObject event=(JSONObject)value,t=tracks.optJSONObject(event.optString("key"));
            if(t==null||!t.optString("provider").equals("Spotify")||event.optLong("ms")<=0||event.optLong("at")>now)continue;
            String id=t.optString("albumId"),name=t.optString("album");
            if(!id.matches("[A-Za-z0-9]{22}")||name.isBlank())continue;
            JSONObject row=albums.computeIfAbsent(id,k->new JSONObject());
            MusicStore.put(row,"id",id);MusicStore.put(row,"name",name);MusicStore.put(row,"type","album");
            MusicStore.put(row,"uri","spotify:album:"+id);
            MusicStore.put(row,"artists",new JSONArray().put(new JSONObject().put("name",t.optString("artist"))));
            if(!t.optString("image").isEmpty())MusicStore.put(row,"images",new JSONArray().put(new JSONObject().put("url",t.optString("image"))));
            MusicStore.put(row,"lastAt",Math.max(row.optLong("lastAt"),event.optLong("lastAt",event.optLong("at"))));
            MusicStore.put(row,"listenedMs",row.optLong("listenedMs")+Math.max(0,event.optLong("ms")));
        }
        ArrayList<JSONObject> ordered=new ArrayList<>(albums.values());
        ordered.sort(Comparator.comparingLong((JSONObject a)->a.optLong("lastAt")).reversed().thenComparing(a->a.optString("id")));
        ArrayList<JSONObject> top=new ArrayList<>(ordered);top.sort(Comparator.comparingLong((JSONObject a)->a.optLong("listenedMs")).reversed());
        ArrayList<JSONObject> result=new ArrayList<>(ordered.subList(0,Math.min(60,ordered.size())));Set<String> ids=new HashSet<>();for(JSONObject a:result)ids.add(a.optString("id"));for(JSONObject a:top.subList(0,Math.min(20,top.size())))if(ids.add(a.optString("id")))result.add(a);
        return new JSONArray(result);
    }
}
