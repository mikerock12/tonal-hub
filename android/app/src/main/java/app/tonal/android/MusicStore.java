package app.tonal.android;

import android.app.Application;
import android.content.Context;
import android.os.*;
import android.util.AtomicFile;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.regex.*;

/** Private listening journal. No tokens, telemetry, lyrics or signed URLs on disk. */
final class MusicStore {
    private static MusicStore instance;
    static synchronized MusicStore get(Context c){if(instance==null)instance=new MusicStore(c);return instance;}
    final Handler main=new Handler(Looper.getMainLooper());
    private final ExecutorService disk=Executors.newSingleThreadExecutor();
    private final AtomicFile file;
    private final PersonalDatabase database;
    JSONObject data=new JSONObject(),current=new JSONObject(),rules=new JSONObject();
    JSONArray lines=new JSONArray();String lyricStatus="Busque a letra desta faixa • LRCLIB";
    boolean busy;int generation,lyricVersion;
    private final String module=Application.getProcessName().endsWith(":indie")?"indie":"tonalify";
    private String session=UUID.randomUUID().toString();private int position;
    private long at=SystemClock.elapsedRealtime(),pending,total;private boolean playing;
    private final Runnable saveTask=this::flush;
    private MusicStore(Context c){this(c,"music-library-"+(Application.getProcessName().endsWith(":indie")?"indie":"tonalify")+"-v1.json");}
    MusicStore(Context c,String filename){
        file=new AtomicFile(new File(c.getFilesDir(),filename));
        database=new PersonalDatabase(c,filename.replace(".json",".sqlite"));
        try{data=database.read();if(data.length()==0&&file.getBaseFile().length()<8*1024*1024)data=new JSONObject(new String(file.readFully(),StandardCharsets.UTF_8));}catch(FileNotFoundException ignored){}catch(Exception error){Diagnostics.write("Biblioteca pessoal: falha ao ler SQLite");}
        try(InputStream in=c.getAssets().open("music-rules.json")){rules=new JSONObject(new String(in.readAllBytes(),StandardCharsets.UTF_8));}catch(Exception ignored){}
    }
    static void put(JSONObject o,String k,Object v){try{o.put(k,v);}catch(JSONException ignored){}}
    static JSONArray array(JSONObject o,String k){JSONArray a=o.optJSONArray(k);return a==null?new JSONArray():a;}
    static JSONObject object(JSONObject o,String k){JSONObject v=o.optJSONObject(k);return v==null?new JSONObject():v;}
    static String key(JSONObject t){return t.optString("provider",t.optString("source"))+"|"+t.optString("server")+"|"+t.optString("account")+"|"+t.optString("id",t.optString("uri"));}
    static JSONObject safe(JSONObject t){JSONObject r=new JSONObject();for(String k:new String[]{"provider","source","id","uri","title","artist","album","genre","image","durationMs","playable","server","account","localFile"})if(t.has(k))put(r,k,t.opt(k));if(!r.has("provider"))put(r,"provider",t.optString("source"));
        try{java.net.URI u=new java.net.URI(t.optString("stream"));if("https".equals(u.getScheme())&&u.getHost()!=null&&u.getUserInfo()==null&&u.getQuery()==null)put(r,"stream",u.toString());}catch(Exception ignored){}return r;}
    static JSONObject spotifyTrack(JSONObject item){JSONObject t=new JSONObject();String uri=item.optString("uri");put(t,"provider","Spotify");put(t,"id",uri);put(t,"uri",uri);put(t,"title",item.optString("name"));put(t,"artist",CatalogScreen.artists(item));put(t,"album",object(item,"album").optString("name"));put(t,"image",Artwork.url(item));put(t,"durationMs",item.optInt("duration_ms"));put(t,"playable",true);return t;}
    void remember(JSONObject track){JSONObject tracks=object(data,"tracks");if(!tracks.has(key(track))&&tracks.length()<3000)put(tracks,key(track),safe(track));put(data,"tracks",tracks);save();}
    void save(){main.removeCallbacks(saveTask);main.postDelayed(saveTask,750);}
    void flush(){main.removeCallbacks(saveTask);String snapshot=data.toString();disk.execute(()->{try{database.write(snapshot);}catch(Exception error){Diagnostics.write("Biblioteca pessoal: falha ao salvar SQLite");}});}
    void select(JSONObject t){
        if(t.optString("title").isEmpty())return;String id=key(t);JSONObject tracks=object(data,"tracks"),old=object(tracks,id),next=safe(t);
        for(String k:new String[]{"genre","tone","lyricsAnalyzed","topics","topicVersion"})if(old.has(k)&&(!next.has(k)||next.optString(k).isEmpty()))put(next,k,old.opt(k));
        if(!id.equals(key(current))){checkpoint();session=UUID.randomUUID().toString();pending=total=0;position=0;playing=false;at=SystemClock.elapsedRealtime();generation++;busy=false;lines=new JSONArray();lyricStatus="Busque a letra desta faixa • LRCLIB";}
        current=next;put(tracks,id,next);
        if(tracks.length()>3000){HashSet<String> keep=new HashSet<>();for(Object v:values(array(data,"favorites")))keep.add(String.valueOf(v));for(Object p:values(array(data,"playlists")))for(Object v:values(array((JSONObject)p,"tracks")))keep.add(String.valueOf(v));ArrayList<String> keys=new ArrayList<>();tracks.keys().forEachRemaining(keys::add);for(String k:keys)if(tracks.length()>3000&&!k.equals(id)&&!keep.contains(k))tracks.remove(k);}
        put(data,"tracks",tracks);save();
    }
    void pulse(boolean active,int pos){long now=SystemClock.elapsedRealtime(),wall=now-at;at=now;int delta=pos-position;
        if(data.optBoolean("history",true)&&playing&&active&&delta>0&&delta<5000){long ms=Math.min(Math.min(2000,Math.max(0,wall)),delta);pending+=ms;total+=ms;if(pending>=5000)checkpoint();}
        int duration=current.optInt("durationMs");if(duration>1000&&position>=duration-1500&&pos<1000){checkpoint();session=UUID.randomUUID().toString();total=0;}
        position=pos;playing=active;if(!active)checkpoint();
    }
    void checkpoint(){if(pending<=0||current.length()==0)return;JSONArray events=array(data,"events");JSONObject e=null;for(int n=events.length()-1;n>=Math.max(0,events.length()-8);n--){JSONObject candidate=events.optJSONObject(n);if(candidate!=null&&candidate.optString("session").equals(session)){e=candidate;break;}}
        if(e==null){e=new JSONObject();put(e,"at",System.currentTimeMillis());events.put(e);}put(e,"session",session);put(e,"key",key(current));put(e,"ms",e.optLong("ms")+pending);int d=current.optInt("durationMs");put(e,"play",total>=(d>0?Math.min(30000,Math.max(1000,d/2)):30000));pending=0;
        long cutoff=System.currentTimeMillis()-180L*86400000;JSONArray kept=new JSONArray();for(int n=Math.max(0,events.length()-10000);n<events.length();n++){JSONObject row=events.optJSONObject(n);if(row!=null&&row.optLong("at")>=cutoff)kept.put(row);}put(data,"events",kept);save();
    }
    boolean liked(JSONObject t){String k=key(t);for(Object v:values(array(data,"favorites")))if(k.equals(v))return true;return false;}
    void favorite(JSONObject t,boolean on){JSONObject tracks=object(data,"tracks"),stored=object(tracks,key(t)),clean=safe(t);clean.keys().forEachRemaining(k->put(stored,k,clean.opt(k)));put(tracks,key(t),stored);put(data,"tracks",tracks);JSONArray next=new JSONArray();if(on)next.put(key(t));for(Object k:values(array(data,"favorites")))if(!key(t).equals(k))next.put(k);put(data,"favorites",next);save();}
    JSONArray favorites(){return resolve(array(data,"favorites"));}
    JSONArray resolve(JSONArray ids){JSONArray tracks=new JSONArray();JSONObject all=object(data,"tracks");for(Object k:values(ids)){JSONObject t=all.optJSONObject(String.valueOf(k));if(t!=null)tracks.put(t);}return tracks;}
    String createPlaylist(String name){if(name.trim().isEmpty()||array(data,"playlists").length()>=100)return "";JSONObject p=new JSONObject();String id=UUID.randomUUID().toString();put(p,"id",id);put(p,"name",name.trim().substring(0,Math.min(100,name.trim().length())));put(p,"tracks",new JSONArray());JSONArray list=array(data,"playlists");list.put(p);put(data,"playlists",list);save();return id;}
    void add(String id,JSONObject t){JSONObject all=object(data,"tracks");JSONObject saved=object(all,key(t)),cleanTrack=safe(t);cleanTrack.keys().forEachRemaining(k->put(saved,k,cleanTrack.opt(k)));put(all,key(t),saved);put(data,"tracks",all);for(Object v:values(array(data,"playlists"))){JSONObject p=(JSONObject)v;if(!id.equals(p.optString("id")))continue;JSONArray tracks=array(p,"tracks");boolean exists=false;for(Object k:values(tracks))exists|=key(t).equals(k);if(!exists&&tracks.length()<1000)tracks.put(key(t));put(p,"tracks",tracks);}save();}
    void remove(String id,int row){for(Object v:values(array(data,"playlists"))){JSONObject p=(JSONObject)v;if(id.equals(p.optString("id")))array(p,"tracks").remove(row);}save();}
    void delete(String id){JSONArray all=array(data,"playlists");for(int i=all.length()-1;i>=0;i--)if(id.equals(all.optJSONObject(i).optString("id")))all.remove(i);put(data,"playlists",all);save();}
    void clearHistory(){pending=total=0;put(data,"events",new JSONArray());save();}
    String exportBackup()throws Exception{checkpoint();return new JSONObject().put("format","tonal-personal-backup").put("version",1).put("module",module).put("createdAt",new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssXXX",Locale.ROOT).format(new Date())).put("data",data).toString(2);}
    void restoreBackup(String text)throws Exception{
        if(text.getBytes(StandardCharsets.UTF_8).length>8*1024*1024)throw new IOException("Backup maior que 8 MiB");JSONObject doc=new JSONObject(text),incoming=doc.getJSONObject("data");JSONObject imported=incoming.getJSONObject("tracks");
        if(!doc.optString("format").equals("tonal-personal-backup")||doc.optInt("version")!=1||!module.equals(doc.optString("module"))||imported.length()>3000||array(incoming,"events").length()>10000||array(incoming,"playlists").length()>100)throw new IOException("Backup incompatível com este player");
        JSONObject clean=new JSONObject(),tracks=new JSONObject();for(Iterator<String> it=imported.keys();it.hasNext();){String k=it.next();JSONObject raw=imported.getJSONObject(k),t=safe(raw);if(!key(t).equals(k))throw new IOException("Identificação de música inválida");JSONObject tone=new JSONObject(),scores=object(raw,"tone");for(String name:new String[]{"intenso","contemplativo","afetivo","festivo"})if(scores.has(name))put(tone,name,Math.max(0,Math.min(11,scores.optDouble(name))));put(t,"tone",tone);put(t,"lyricsAnalyzed",raw.optBoolean("lyricsAnalyzed"));JSONObject topics=new JSONObject();for(String topic:MusicInsights.TOPICS){double score=object(raw,"topics").optDouble(topic,-1);if(Double.isFinite(score)&&score>=0)put(topics,topic,Math.min(1,score));}put(t,"topics",topics);put(t,"topicVersion",1);put(tracks,k,t);}
        put(clean,"tracks",tracks);JSONArray favorites=new JSONArray();for(Object k:values(array(incoming,"favorites")))if(k instanceof String&&tracks.has((String)k)&&favorites.length()<3000)favorites.put(k);put(clean,"favorites",favorites);
        JSONArray playlists=new JSONArray();for(Object value:values(array(incoming,"playlists"))){JSONObject raw=(JSONObject)value,p=new JSONObject();put(p,"id",raw.getString("id"));put(p,"name",raw.optString("name").substring(0,Math.min(100,raw.optString("name").length())));JSONArray entries=new JSONArray();HashSet<String> seen=new HashSet<>();for(Object k:values(array(raw,"tracks")))if(k instanceof String&&tracks.has((String)k)&&entries.length()<1000&&seen.add((String)k))entries.put(k);put(p,"tracks",entries);playlists.put(p);}put(clean,"playlists",playlists);
        JSONArray events=new JSONArray();for(Object value:values(array(incoming,"events"))){JSONObject raw=(JSONObject)value;if(!tracks.has(raw.optString("key")))continue;JSONObject e=new JSONObject();put(e,"key",raw.optString("key"));put(e,"session",raw.optString("session",UUID.randomUUID().toString()));put(e,"at",raw.optLong("at"));put(e,"ms",Math.max(0,Math.min(86400000,raw.optLong("ms"))));put(e,"play",raw.optBoolean("play"));events.put(e);}put(clean,"events",events);
        for(String k:new String[]{"history","background","photo","translationDay","translationBytes","searches","lyricStyle"})if(incoming.has(k))put(clean,k,incoming.opt(k));checkpoint();data=clean;JSONArray validSearches=new JSONArray();for(Object q:values(searches()))if(q instanceof String&&validSearches.length()<20)validSearches.put(((String)q).substring(0,Math.min(200,((String)q).length())));put(data,"searches",validSearches);put(data,"lyricStyle",lyricStyle());pending=total=0;session=UUID.randomUUID().toString();playing=false;at=SystemClock.elapsedRealtime();generation++;lyricVersion++;lines=new JSONArray();busy=false;save();
    }
    static List<Object> values(JSONArray a){ArrayList<Object> r=new ArrayList<>();for(int i=0;i<a.length();i++)r.add(a.opt(i));return r;}
    JSONObject tone(String text,String genre){JSONObject result=new JSONObject(),defs=object(rules,"tones");List<String> words=Arrays.asList(IndieCatalog.normalize(text).split(" "));Iterator<String> it=defs.keys();while(it.hasNext()){String k=it.next();JSONObject d=object(defs,k);int hits=0;HashSet<String> lex=new HashSet<>();for(Object v:values(array(d,"words")))lex.add(IndieCatalog.normalize(String.valueOf(v)));for(String w:words)if(lex.contains(w))hits++;int bonus=0;for(Object g:values(array(d,"genres")))if(IndieCatalog.normalize(genre).contains(IndieCatalog.normalize(String.valueOf(g))))bonus=1;put(result,k,Math.min(10,hits)+bonus);}return result;}
    void analyze(){StringBuilder text=new StringBuilder();for(Object v:values(lines))text.append(((JSONObject)v).optString("text")).append('\n');put(current,"tone",tone(text.toString(),current.optString("genre")));put(current,"lyricsAnalyzed",!text.toString().trim().isEmpty());JSONObject topics=text.toString().trim().isEmpty()?new JSONObject():MusicInsightModel.analyze(text.toString());put(current,"topics",topics);put(current,"topicVersion",1);JSONObject all=object(data,"tracks");put(all,key(current),current);put(data,"tracks",all);save();}
    JSONObject statistics(int days,String dimension){return MusicInsights.report(this,days,dimension);}
    JSONObject insights(int days){return MusicInsights.details(this,days);}
    JSONArray searches(){return array(data,"searches");}
    void rememberSearch(String query){String q=query.trim();if(q.isEmpty())return;q=q.substring(0,Math.min(200,q.length()));JSONArray a=new JSONArray();a.put(q);for(Object v:values(searches()))if(v instanceof String&&!q.equalsIgnoreCase((String)v)&&a.length()<20)a.put(v);put(data,"searches",a);save();}
    void forgetSearch(String query){JSONArray a=new JSONArray();for(Object v:values(searches()))if(!query.isEmpty()&&!query.equals(v))a.put(v);put(data,"searches",a);save();}
    JSONObject lyricStyle(){JSONObject s=new JSONObject();put(s,"lyricFont","Sans");put(s,"translationFont","Sans");put(s,"lyricColor","#F5F5F5");put(s,"translationColor","#CEE9F0");put(s,"lyricSize",26);put(s,"translationSize",21);JSONObject saved=object(data,"lyricStyle");for(Iterator<String> it=saved.keys();it.hasNext();){String k=it.next();Object v=validStyle(k,saved.opt(k));if(s.has(k)&&v!=null)put(s,k,v);}return s;}
    static Object validStyle(String key,Object value){if(key.endsWith("Font"))return Arrays.asList("Sans","Serif","Mono","Rounded").contains(value)?value:null;if(key.endsWith("Color"))return String.valueOf(value).matches("#[0-9a-fA-F]{6}")?value:null;if(key.endsWith("Size")&&value instanceof Number)return Math.max(16,Math.min(44,((Number)value).intValue()));return null;}
    void setLyricStyle(String key,Object value){JSONObject s=lyricStyle();Object v=validStyle(key,value);if(!s.has(key)||v==null)return;put(s,key,v);put(data,"lyricStyle",s);save();}
    static JSONArray parseLyrics(String text){JSONArray result=new JSONArray();Pattern stamp=Pattern.compile("\\[(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?\\]");Matcher offsetMatcher=Pattern.compile("\\[offset:([+-]?\\d+)\\]",Pattern.CASE_INSENSITIVE).matcher(text);int offset=offsetMatcher.find()?Integer.parseInt(offsetMatcher.group(1)):0;ArrayList<JSONObject> rows=new ArrayList<>();boolean synced=false;
        for(String raw:text.substring(0,Math.min(100000,text.length())).split("\n",-1)){Matcher m=stamp.matcher(raw);ArrayList<Integer> times=new ArrayList<>();int end=0;while(m.find()){String fraction=m.group(3)==null?"0":m.group(3);fraction=(fraction+"000").substring(0,3);times.add(Math.max(0,Integer.parseInt(m.group(1))*60000+Integer.parseInt(m.group(2))*1000+Integer.parseInt(fraction)+offset));end=m.end();}String line=raw.substring(end).trim();if(!times.isEmpty())synced=true;if(times.isEmpty()&&line.startsWith("["))continue;if(times.isEmpty())times.add(-1);for(int time:times){JSONObject row=new JSONObject();put(row,"time",time);put(row,"text",line);rows.add(row);}if(rows.size()>=1000)break;}
        if(synced){rows.removeIf(r->r.optInt("time")<0);rows.sort(Comparator.comparingInt(r->r.optInt("time")));}for(JSONObject row:rows)result.put(row);return result;
    }
}
