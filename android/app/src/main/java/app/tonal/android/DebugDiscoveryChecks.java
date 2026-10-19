package app.tonal.android;
import org.json.*;
import java.util.*;

/** Native JNI regression fixtures. Never executes in release builds. */
final class DebugDiscoveryChecks {
    private static void require(boolean ok,String why){if(!ok)throw new AssertionError(why);}
    static JSONObject run(SpotifyClient api,MusicStore store)throws Exception{
        if(!BuildConfig.DEBUG)throw new IllegalStateException("Debug only");
        String previous=store.discoverySourceSignature;ArrayList<JSONObject> accepted=new ArrayList<>(store.discoveryAccepted);
        ArrayList<JSONObject> original=new ArrayList<>();
        for(int i=0;i<8;i++){
            JSONObject t=new JSONObject().put("uri","fixture:retained-"+i).put("name","Retained "+i)
                .put("artists",new JSONArray().put(new JSONObject().put("name",i==2||i==5?"New Band "+i:"Source Band")))
                .put("album",new JSONObject().put("id","album").put("name","Album").put("release_date","1994"))
                .put("duration_ms",180000).put("_mixExternal",i==2||i==5).put("_mixDiscoveryEnabled",true);
            original.add(t);
        }
        ListeningMix mixer=new ListeningMix(api,store);ArrayList<JSONObject> result=new ArrayList<>();int[] checks={0};
        try{
            for(int n=0;n<20;n++){
                result.clear();mixer.prepare(original,0,(n&1)!=0,true,false,result::addAll,text->{});
                require(result.size()==8,"retained queue lost entries");HashSet<String> ids=new HashSet<>();int outside=0;
                for(JSONObject t:result){ids.add(t.optString("uri"));if(t.optBoolean("_mixExternal"))outside++;require(t.optBoolean("_mixDiscoveryEnabled"),"mode state lost");}
                require(ids.size()==8&&outside==2&&result.get(0).optString("uri").equals("fixture:retained-0"),"JNI retention or anchor failed");checks[0]++;
            }
            result.clear();mixer.prepare(original,0,true,false,false,result::addAll,text->{});
            require(result.size()==6&&result.stream().noneMatch(t->t.optBoolean("_mixExternal")),"toggle did not remove discoveries");checks[0]++;
            require(original.get(2).optBoolean("_mixExternal")&&original.get(0).optBoolean("_mixDiscoveryEnabled"),"preview mutated active queue");checks[0]++;
            JSONObject track=new JSONObject().put("uri","spotify:track:AAAAAAAAAAAAAAAAAAAAAA").put("title","Example").put("artist","Band").put("album","Edition").put("durationMs",180000);
            JSONObject lyrics=new JSONObject().put("trackName","Example").put("artistName","Band").put("duration",180);
            require(DiscoveryLyrics.matches(track,lyrics),"exact recording rejected");lyrics.put("duration",210);require(!DiscoveryLyrics.matches(track,lyrics),"different duration accepted");checks[0]++;
            String key=DiscoveryLyrics.signature(track);track.put("album","Other edition");require(!key.equals(DiscoveryLyrics.signature(track)),"cache identity ignores edition");checks[0]++;
            return new JSONObject().put("passed",true).put("checks",checks[0]).put("actual_cpp_jni",true).put("network_requests",0).put("playback_commands",0);
        }finally{mixer.cancel();store.discoverySourceSignature=previous;store.discoveryAccepted.clear();store.discoveryAccepted.addAll(accepted);}
    }
}
