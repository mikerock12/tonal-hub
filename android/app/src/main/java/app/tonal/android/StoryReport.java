package app.tonal.android;
import org.json.*;
import java.util.HashSet;

/** Validates the exact batch before attaching local track metadata. */
final class StoryReport {
    static JSONObject decode(JSONObject report,JSONArray tracks){
        try {
            JSONArray rows=report.optJSONArray("items");int count=tracks.length();
            if(!report.optString("format").equals("lyric-reading-v5")||!(report.opt("complete") instanceof Boolean)||!(report.opt("resumable") instanceof Boolean)||report.optInt("schema")!=3||!report.optString("error").isEmpty()||count<1||count>8||
                report.optInt("total")!=count||rows==null||rows.length()!=count)return null;
            HashSet<Integer> seen=new HashSet<>();JSONArray items=new JSONArray();int accepted=0;boolean pending=false;
            for(int n=0;n<count;n++){
                JSONObject item=rows.getJSONObject(n);double index=item.getDouble("track");int at=(int)index;
                String code=item.getString("code"),theme=item.getString("theme"),meaning=item.getString("meaning");
                if(!(item.get("accepted") instanceof Boolean)||!(item.get("track") instanceof Number))return null;
                boolean ok=item.getBoolean("accepted");if(!(item.opt("pending") instanceof Boolean))return null;boolean waiting=item.getBoolean("pending");pending|=waiting;
                if(index!=at||at<0||at>=count||!seen.add(at)||code.length()!=1||
                    (ok?!code.equals("A"):!code.equals("L"))||(ok&&waiting)||(ok&&(!(item.opt("reviewed") instanceof Boolean)||!item.optBoolean("reviewed")))||
                    theme.trim().isEmpty()||theme.length()>80||meaning.trim().length()<20||meaning.length()>512||meaning.getBytes(java.nio.charset.StandardCharsets.UTF_8).length>700)return null;
                if(ok)accepted++;JSONObject track=tracks.getJSONObject(at);
                items.put(new JSONObject().put("title",bound(track.optString("title"),240))
                    .put("artist",bound(track.optString("artist"),160)).put("accepted",ok).put("pending",waiting).put("theme",theme).put("meaning",meaning));
            }
            String summary=report.getString("summary").trim();
            if(report.getBoolean("complete")==pending||report.getBoolean("resumable")!=pending||report.getInt("qualified")!=accepted||(accepted>0?summary.length()<20:!summary.isEmpty())||summary.length()>1400)return null;
            String stage=report.getString("progressStage"),notice=report.optString("notice");double done=report.getDouble("progressDone"),total=report.getDouble("progressTotal"),track=report.getDouble("progressTrack");
            if(stage.length()>80||notice.length()>400||!(report.get("progressDone") instanceof Number)||!(report.get("progressTotal") instanceof Number)||!(report.get("progressTrack") instanceof Number)||done!=(int)done||total!=(int)total||track!=(int)track||done<0||total<done||total>768||track<0||track>=count)return null;
            return new JSONObject().put("summary",summary).put("complete",!pending).put("resumable",pending).put("notice",notice).put("items",items).put("qualified",accepted).put("total",count);
        }catch(Exception invalid){return null;}
    }
    private static String bound(String value,int size){return value.substring(0,Math.min(size,value.length()));}
}
