package app.tonal.android;

import org.json.*;
import java.util.*;

/** Bounded aggregation of the local journal; never executes on the audio thread. */
final class MusicInsights {
    static final String[] TOPICS={"amor","saudade","perda","rebeldia","esperanca","celebracao","tensao","introspeccao"};
    private static void put(JSONObject o,String k,Object v){MusicStore.put(o,k,v);}
    static long cutoff(int days){Calendar c=Calendar.getInstance();c.set(Calendar.HOUR_OF_DAY,0);c.set(Calendar.MINUTE,0);c.set(Calendar.SECOND,0);c.set(Calendar.MILLISECOND,0);c.add(Calendar.DAY_OF_YEAR,1-Math.max(1,Math.min(180,days)));return c.getTimeInMillis();}
    static JSONObject report(MusicStore store,int days,String dimension){return report(store,days,dimension,30);}
    static JSONObject report(MusicStore store,int days,String dimension,int limit){
        long cutoff=cutoff(days),now=System.currentTimeMillis(),ms=0;int plays=0;
        JSONObject all=MusicStore.object(store.data,"tracks");TreeMap<String,JSONObject> groups=new TreeMap<>();
        Set<String> heard=new HashSet<>(),analyzed=new HashSet<>(),artists=new HashSet<>(),albums=new HashSet<>();
        Map<String,Double> tones=new TreeMap<>();StringBuilder genres=new StringBuilder();
        for(Object value:MusicStore.values(MusicStore.array(store.data,"events"))){
            JSONObject e=(JSONObject)value;if(e.optLong("at")<cutoff||e.optLong("at")>now)continue;
            String key=e.optString("key");JSONObject t=all.optJSONObject(key);if(t==null)continue;
            long time=Math.max(0,Math.min(86400000,e.optLong("ms")));int play=e.optBoolean("play")?1:0;
            String artist=t.optString("artist"),album=t.optString("album"),albumKey=artist+"|"+album;
            String label=dimension.equals("artists")?artist:dimension.equals("albums")?album:dimension.equals("genres")?t.optString("genre"):dimension.equals("days")?new java.text.SimpleDateFormat("yyyy-MM-dd",Locale.ROOT).format(new Date(e.optLong("at"))):t.optString("title");
            boolean missing=label.isEmpty();if(missing)label="Metadados pendentes";String id=dimension.equals("tracks")?key:dimension.equals("albums")?albumKey:label;
            JSONObject r=groups.get(id);if(r==null){r=new JSONObject();put(r,"label",label);put(r,"id",id);groups.put(id,r);}
            put(r,"ms",r.optLong("ms")+time);put(r,"plays",r.optInt("plays")+play);
            JSONObject totals=MusicStore.object(r,"trackTotals"),entry=totals.optJSONObject(key);
            if(entry==null){try{entry=new JSONObject(t.toString());}catch(Exception ignored){entry=new JSONObject();}}
            put(entry,"listenedMs",entry.optLong("listenedMs")+time);put(entry,"listenedPlays",entry.optInt("listenedPlays")+play);put(totals,key,entry);put(r,"trackTotals",totals);
            JSONObject albumTotals=MusicStore.object(r,"albumTotals"),ar=MusicStore.object(albumTotals,albumKey);
            put(ar,"album",album);put(ar,"artist",artist);put(ar,"ms",ar.optLong("ms")+time);put(ar,"plays",ar.optInt("plays")+play);
            JSONObject ids=MusicStore.object(ar,"trackIds");put(ids,key,true);put(ar,"trackIds",ids);put(ar,"trackCount",ids.length());put(albumTotals,albumKey,ar);put(r,"albumTotals",albumTotals);
            ms+=time;plays+=play;boolean fresh=heard.add(key);if(!artist.isEmpty())artists.add(artist);if(!album.isEmpty())albums.add(albumKey);if(t.optBoolean("lyricsAnalyzed"))analyzed.add(key);
            if(fresh)genres.append(' ').append(IndieCatalog.normalize(t.optString("genre")));
            JSONObject scores=t.optJSONObject("tone");if(scores==null)scores=store.tone("",t.optString("genre"));for(Iterator<String> it=scores.keys();it.hasNext();){String k=it.next();tones.put(k,tones.getOrDefault(k,0.)+time*scores.optDouble(k));}
        }
        ArrayList<JSONObject> sorted=new ArrayList<>(groups.values());sorted.removeIf(r->r.optString("label").equals("Metadados pendentes"));for(JSONObject r:sorted){ArrayList<JSONObject> entries=new ArrayList<>();JSONObject totals=MusicStore.object(r,"trackTotals");totals.keys().forEachRemaining(k->entries.add(totals.optJSONObject(k)));entries.sort(Comparator.comparingLong((JSONObject t)->t.optLong("listenedMs")).reversed());put(r,"tracks",new JSONArray(entries));JSONObject als=MusicStore.object(r,"albumTotals");JSONArray albumRows=new JSONArray();als.keys().forEachRemaining(k->{JSONObject a=als.optJSONObject(k);if(!a.optString("album").isEmpty())albumRows.put(a);});put(r,"albums",albumRows);put(r,"trackCount",entries.size());put(r,"albumCount",albumRows.length());r.remove("trackTotals");r.remove("albumTotals");}
        if(!dimension.equals("days"))sorted.sort(Comparator.comparingLong((JSONObject r)->r.optLong("ms")).reversed());
        JSONObject report=new JSONObject();put(report,"rows",new JSONArray(sorted.subList(0,Math.min(Math.max(1,Math.min(3000,limit)),sorted.size()))));put(report,"ms",ms);put(report,"plays",plays);put(report,"coverage",heard.size());put(report,"lyricCoverage",analyzed.size());put(report,"artistCount",artists.size());put(report,"albumCount",albums.size());
        String dominant="";double best=0;for(Map.Entry<String,Double> e:tones.entrySet())if(e.getValue()>best){dominant=e.getKey();best=e.getValue();}
        put(report,"vibe",plays>=3&&!analyzed.isEmpty()&&!dominant.isEmpty()?"Clima "+dominant:"Dados insuficientes");
        put(report,"explanation",plays+" escutas • "+heard.size()+" músicas • "+artists.size()+" artistas • "+albums.size()+" álbuns. Letras analisadas: "+analyzed.size()+"/"+heard.size()+". Toque para explorar temas e detalhes.");
        JSONArray recommendations=new JSONArray();Set<String> seen=new HashSet<>();Set<String> known=new HashSet<>();for(String a:artists)known.add(IndieCatalog.normalize(a));for(Object value:MusicStore.values(MusicStore.array(store.rules,"recommendations"))){JSONObject rule=(JSONObject)value;boolean match=false;for(Object g:MusicStore.values(MusicStore.array(rule,"genres")))match|=genres.toString().contains(IndieCatalog.normalize(String.valueOf(g)));if(match)for(Object a:MusicStore.values(MusicStore.array(rule,"artists")))if(recommendations.length()<12&&!known.contains(IndieCatalog.normalize(String.valueOf(a)))&&seen.add(String.valueOf(a)))recommendations.put(a);}put(report,"recommendations",recommendations);return report;
    }
    // Build one index per journal revision, never scan the journal once per list row.
    private static MusicStore indexed;private static int revision=-1;private static final Map<String,JSONObject> index=new HashMap<>();
    static JSONObject summary(MusicStore store,String kind,String value){
        if(indexed!=store||revision!=store.revision){indexed=store;revision=store.revision;index.clear();JSONObject tracks=MusicStore.object(store.data,"tracks");
            for(Object v:MusicStore.values(MusicStore.array(store.data,"events"))){JSONObject e=(JSONObject)v,t=tracks.optJSONObject(e.optString("key"));if(t==null)continue;
                for(String dimension:new String[]{"track","artist","album"}){String id=dimension.equals("track")?MusicStore.key(t):dimension.equals("artist")?t.optString("artist"):t.optString("artist")+"|"+t.optString("album");if(id.isEmpty())continue;String k=dimension+"|"+id;JSONObject r=index.get(k);if(r==null){r=new JSONObject();index.put(k,r);}put(r,"ms",r.optLong("ms")+Math.max(0,Math.min(86400000,e.optLong("ms"))));put(r,"plays",r.optInt("plays")+(e.optBoolean("play")?1:0));JSONObject ids=MusicStore.object(r,"ids");put(ids,MusicStore.key(t),true);put(r,"ids",ids);put(r,"tracks",ids.length());}
            }
        }return index.getOrDefault(kind+"|"+value,new JSONObject());
    }
    static JSONObject details(MusicStore store,int days){
        JSONObject r=report(store,days,"artists");put(r,"artists",r.opt("rows"));put(r,"albums",report(store,days,"albums").opt("rows"));put(r,"genres",report(store,days,"genres").opt("rows"));
        long cutoff=cutoff(days),now=System.currentTimeMillis(),ms=0;Map<String,Long> sums=new TreeMap<>();Set<String> coverage=new HashSet<>();JSONObject tracks=MusicStore.object(store.data,"tracks");
        for(Object value:MusicStore.values(MusicStore.array(store.data,"events"))){JSONObject e=(JSONObject)value;if(e.optLong("at")<cutoff||e.optLong("at")>now)continue;JSONObject topics=MusicStore.object(MusicStore.object(tracks,e.optString("key")),"topics");if(topics.length()==0)continue;long time=Math.max(0,Math.min(86400000,e.optLong("ms")));ms+=time;coverage.add(e.optString("key"));for(String k:TOPICS)if(topics.optDouble(k)>=.60)sums.put(k,sums.getOrDefault(k,0L)+time);}
        ArrayList<JSONObject> themes=new ArrayList<>();for(Map.Entry<String,Long> e:sums.entrySet()){JSONObject t=new JSONObject();put(t,"label",e.getKey());put(t,"percent",ms>0?100.*e.getValue()/ms:0);themes.add(t);}themes.sort(Comparator.comparingDouble((JSONObject t)->t.optDouble("percent")).reversed());put(r,"themes",new JSONArray(themes));put(r,"modelCoverage",coverage.size());put(r,"model","TonalMuse Tiny v1 • classificador local C++ • 32 KiB de pesos");return r;
    }
}
