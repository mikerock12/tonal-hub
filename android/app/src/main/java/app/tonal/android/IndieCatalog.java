package app.tonal.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.*;
import org.json.*;
import java.io.*;
import java.security.*;
import java.text.Normalizer;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** Independent public catalog. Spotify objects and credentials never enter this class. */
final class IndieCatalog implements AutoCloseable {
    static final String[] SOURCES={"Todas as fontes","Audius","MusicBrainz","Internet Archive","Aadam Jacobs","Live Music Archive","Jamendo","OpenSubsonic"};
    static final class Item {
        String source,id,kind,title,artist="",detail="",image="",album="",genre="",stream="",server="",account="";
        boolean playable,related,verified,curated;int apiScore,duration;
        Item(String s,String i,String k,String t){source=s;id=i;kind=k;title=t;}
        String key(){return source+"|"+kind+"|"+id;}
        JSONObject saved(){JSONObject o=json();for(String k:new String[]{"image","stream","album","genre","server","account"}){String v=k.equals("image")?image:k.equals("stream")?stream:k.equals("album")?album:k.equals("genre")?genre:k.equals("server")?server:account;MusicStore.put(o,k,v);}MusicStore.put(o,"durationMs",duration);return MusicStore.safe(o);}
        static Item restore(JSONObject o){Item i=new Item(o.optString("provider",o.optString("source")),o.optString("id"),"track",o.optString("title"));i.artist=o.optString("artist");i.album=o.optString("album");i.genre=o.optString("genre");i.image=o.optString("image");i.stream=o.optString("stream");i.server=o.optString("server");i.account=o.optString("account");i.duration=o.optInt("durationMs");i.playable=o.optBoolean("playable");return i;}

        JSONObject json(){JSONObject o=new JSONObject();try{o.put("source",source).put("id",id).put("kind",kind).put("title",title).put("artist",artist).put("detail",detail).put("playable",playable).put("related",related);}catch(JSONException ignored){}return o;}
    }
    static final class Batch {
        final String source;final ArrayList<Item> items=new ArrayList<>();String error="";boolean more,stale;
        Batch(String s){source=s;}
    }
    private final ExecutorService workers=Executors.newFixedThreadPool(4);
    private final Handler main=new Handler(Looper.getMainLooper());
    private final SharedPreferences prefs;private final Secrets secrets;
    private volatile int generation;private volatile boolean closed;
    private final Object mbLock=new Object();private long nextMb;private int cacheBytes;
    private static final class Cached {final JSONObject json;final long at=System.currentTimeMillis();final int size;Cached(JSONObject j){json=j;size=j.toString().length()*2;}}
    private final LinkedHashMap<String,Cached> cache=new LinkedHashMap<>(16,.75f,true);
    IndieCatalog(Context c){prefs=c.getSharedPreferences("indie-connections",0);secrets=new Secrets(c,"indie-library");}
    SharedPreferences preferences(){return prefs;}
    JSONObject credentials(){JSONObject o=secrets.load();return o==null?new JSONObject():o;}
    void saveConnections(String jamendo,String server,String user,String password)throws Exception{
        server=server.trim().replaceAll("/+$","");user=user.trim();if(!server.isEmpty())WebRequest.url(server);
        JSONObject old=credentials();boolean same=server.equals(old.optString("server"))&&user.equals(old.optString("user"));
        JSONObject value=new JSONObject().put("server",server).put("user",user).put("password",password.isEmpty()&&same?old.optString("password"):password);
        secrets.save(value);prefs.edit().putString("jamendo",jamendo.trim()).apply();generation++;
    }
    void load(String query,String source,Item route,int page,Consumer<Batch> sink){
        int epoch=++generation;
        for(String provider:SOURCES){if(provider.equals(SOURCES[0]))continue;if(route==null&&source.equals(SOURCES[0])&&(provider.equals("Live Music Archive")||provider.equals("Aadam Jacobs")))continue;if(route!=null&&!provider.equals(route.source))continue;if(route==null&&!source.equals(SOURCES[0])&&!provider.equals(source))continue;
            if(provider.equals("Jamendo")&&prefs.getString("jamendo","").isEmpty()){if(source.equals(provider))deliver(epoch,sink,error(provider,"Cadastre seu client ID ativo em Conexões."));continue;}
            if(provider.equals("OpenSubsonic")&&credentials().optString("server").isEmpty()){if(source.equals(provider))deliver(epoch,sink,error(provider,"Conecte sua biblioteca HTTPS em Conexões."));continue;}
            workers.execute(()->{Batch batch=new Batch(provider);try{if(epoch!=generation)return;switch(provider){case "Audius":audius(batch,query,route,page);break;case "MusicBrainz":musicbrainz(batch,query,route,page);break;case "Internet Archive":case "Aadam Jacobs":case "Live Music Archive":archive(batch,query,route,page);break;case "Jamendo":jamendo(batch,query,route,page);break;case "OpenSubsonic":subsonic(batch,query,route,page);break;}}catch(Exception e){batch.error=e instanceof WebRequest.Failure?"Consulta indisponível ("+e.getMessage()+"). Tente novamente.":"Não foi possível carregar esta fonte. Verifique a conexão e tente novamente.";Diagnostics.write(provider+" catálogo: "+e.getClass().getSimpleName()+" "+(e instanceof WebRequest.Failure?e.getMessage():""));}deliver(epoch,sink,batch);});
        }
    }
    private Batch error(String source,String error){Batch b=new Batch(source);b.error=error;return b;}
    private void deliver(int epoch,Consumer<Batch> sink,Batch b){main.post(()->{if(!closed&&generation==epoch)sink.accept(b);});}
    private JSONObject request(Batch b,String url)throws Exception{
        if(!b.source.equals("MusicBrainz"))return WebRequest.json(url);
        synchronized(mbLock){Cached stored=cache.get(url);long age=stored==null?Long.MAX_VALUE:System.currentTimeMillis()-stored.at;if(age<900000)return stored.json;
            Exception failure=null;
            for(int attempt=0;attempt<3;attempt++){
                long wait=nextMb-SystemClock.elapsedRealtime();if(wait>0)Thread.sleep(wait);nextMb=SystemClock.elapsedRealtime()+1100;
                try{JSONObject result=WebRequest.json(url);Cached entry=new Cached(result);if(entry.size<=8*1024*1024){Cached old=cache.put(url,entry);cacheBytes+=entry.size-(old==null?0:old.size);while(cacheBytes>8*1024*1024){String first=cache.keySet().iterator().next();cacheBytes-=cache.remove(first).size;}}return result;}catch(Exception e){failure=e;boolean transientError=!(e instanceof WebRequest.Failure)||Arrays.asList(429,500,502,503,504).contains(((WebRequest.Failure)e).code);if(!transientError)throw e;if(attempt==2)break;long delay=attempt==0?2000:5000;if(e instanceof WebRequest.Failure){String retry=((WebRequest.Failure)e).retryAfter;try{delay=Math.max(delay,Long.parseLong(retry)*1000);}catch(NumberFormatException ignored){try{delay=Math.max(delay,java.time.ZonedDateTime.parse(retry,java.time.format.DateTimeFormatter.RFC_1123_DATE_TIME).toInstant().toEpochMilli()-System.currentTimeMillis());}catch(Exception unused){}}}if(delay>30000)break;Thread.sleep(delay);}
            }
            if(stored!=null&&age<86400000){b.stale=true;return stored.json;}throw failure;
        }
    }
    private static JSONArray array(JSONObject o,String key){JSONArray a=o.optJSONArray(key);return a==null?new JSONArray():a;}
    private static String name(JSONObject o,String key,String fallback){String n=o.optString(key);return n.isEmpty()?fallback:n;}
    private static String routeArtist(Item r){return r==null?"":r.artist.isEmpty()?r.title:r.artist;}
    private void audius(Batch b,String query,Item route,int page)throws Exception{
        if(route==null){for(String kind:new String[]{"artist","album","track"}){String path=kind.equals("artist")?"users":kind.equals("album")?"playlists":"tracks";JSONObject o=request(b,WebRequest.params("https://api.audius.co/v1/"+path+"/search","query",query,"limit","20","offset",""+page*20,"app_name","IndieTonal"));JSONArray a=array(o,"data");b.more|=a.length()==20;parseAudius(b,a,kind);}return;}
        String path=route.kind.equals("artist")?"users/"+route.id+"/tracks":"playlists/"+route.id+"/tracks";
        JSONObject o=request(b,WebRequest.params("https://api.audius.co/v1/"+path,"limit","20","offset",""+page*20,"app_name","IndieTonal"));JSONArray a=array(o,"data");b.more=a.length()==20;parseAudius(b,a,"track");
    }
    private void parseAudius(Batch b,JSONArray a,String kind){for(int n=0;n<a.length();n++){JSONObject t=a.optJSONObject(n);if(t==null)continue;if(t.has("tracks")){parseAudius(b,array(t,"tracks"),"track");continue;}JSONObject user=t.optJSONObject("user");Item i=new Item(b.source,t.optString("id"),kind,t.optString(kind.equals("artist")?"name":kind.equals("album")?"playlist_name":"title"));i.artist=user==null?"":user.optString("name");if(kind.equals("album")&&!t.optBoolean("is_album"))i.kind="playlist";i.verified=t.optBoolean("is_verified");JSONObject art=t.optJSONObject(kind.equals("artist")?"profile_picture":"artwork");if(art!=null)i.image=art.optString("150x150",art.optString("480x480"));i.duration=t.optInt("duration")*1000;i.genre=t.optString("genre");i.album=t.optString("album_name",t.optString("album"));i.playable=kind.equals("track")&&t.optBoolean("is_streamable")&&!t.optBoolean("is_stream_gated")&&!t.optBoolean("is_delete");i.detail=kind.equals("track")?(i.playable?"Reprodução pública completa":"Acesso restrito / indisponível"):"Perfil / coleção Audius • confira a identidade";if(i.playable)i.stream="https://api.audius.co/v1/tracks/"+WebRequest.encode(i.id)+"/stream?app_name=IndieTonal";b.items.add(i);}}
    private void musicbrainz(Batch b,String query,Item route,int page)throws Exception{
        String base="https://musicbrainz.org/ws/2/",kind;String url;
        if(route==null){kind="artist";url=WebRequest.params(base+"artist","query","artist:\""+query.replace("\"","")+"\"","fmt","json","limit","20","offset",""+page*20);}
        else if(route.kind.equals("artist")){kind="album";url=WebRequest.params(base+"release-group","artist",route.id,"fmt","json","limit","20","offset",""+page*20);}
        else if(route.kind.equals("album")){kind="release";url=WebRequest.params(base+"release-group/"+route.id,"inc","releases","fmt","json");}
        else{kind="track";url=WebRequest.params(base+"release/"+route.id,"inc","recordings+artist-credits+isrcs","fmt","json");}
        JSONObject o=request(b,url);JSONArray a=array(o,kind.equals("artist")?"artists":kind.equals("album")?"release-groups":"releases");
        b.more=(kind.equals("artist")?o.optInt("count"):kind.equals("album")?o.optInt("release-group-count"):0)>(page+1)*20;
        if(kind.equals("track")){a=new JSONArray();JSONArray media=array(o,"media");for(int m=0;m<media.length();m++){JSONArray tracks=array(media.getJSONObject(m),"tracks");for(int n=0;n<tracks.length();n++){JSONObject recording=tracks.getJSONObject(n).optJSONObject("recording");if(recording!=null)a.put(recording);}}}
        for(int n=0;n<a.length();n++){JSONObject t=a.getJSONObject(n);Item i=new Item(b.source,t.optString("id"),kind,t.optString(kind.equals("artist")?"name":"title"));i.artist=routeArtist(route);i.apiScore=t.optInt("score");i.detail="Somente discografia • "+t.optString("country")+" • "+t.optString("disambiguation")+" • "+t.optString("first-release-date",t.optString("date"));b.items.add(i);}
    }
    static String recordingKind(JSONObject d){
        String title=normalize(d.optString("title"));
        if(title.matches(".*\\b(demo|demos|rehearsal|rehearsals|outtakes|ensaio)\\b.*"))return "demo";
        if(d.optString("mediatype").equals("etree")||d.optString("identifier").startsWith("ajc")||title.matches(".*\\b(live|show|concert|gig|audience|soundboard)\\b.*")||d.optString("title").matches(".*\\b(19|20)\\d{2}[- .]\\d{2}[- .]\\d{2}\\b.*"))return "show";
        return "recording";
    }
    static int formatPriority(String format){String f=format.toLowerCase(Locale.ROOT);return f.contains("mp3")?3:f.contains("ogg")?2:f.contains("flac")?1:0;}
    private void archive(Batch b,String query,Item route,int page)throws Exception{
        if(route==null){
            String q=query.replace("\\","\\\\").replace("\"","\\\"");
            String scope=b.source.equals("Live Music Archive")?"collection:etree AND mediatype:etree":b.source.equals("Aadam Jacobs")?"collection:aadamjacobs AND mediatype:audio":"(mediatype:audio OR mediatype:etree)";
            JSONObject o=request(b,WebRequest.params("https://archive.org/advancedsearch.php","q",scope+" AND (creator:\""+q+"\" OR title:\""+q+"\")","output","json","rows","20","page",""+(page+1),"sort[]","downloads desc","fl[]","identifier,title,creator,date,mediatype"));
            JSONObject response=o.getJSONObject("response");JSONArray a=array(response,"docs");b.more=response.optInt("numFound")>(page+1)*20;
            for(int n=0;n<a.length();n++){
                JSONObject t=a.getJSONObject(n);String media=t.optString("mediatype");if((!media.equals("etree")&&!media.equals("audio"))||t.optBoolean("is_dark")||t.optBoolean("access-restricted-item"))continue;
                Item i=new Item(b.source,t.optString("identifier"),recordingKind(t),t.optString("title"));Object creator=t.opt("creator");i.artist=creator instanceof JSONArray?((JSONArray)creator).join(", ").replace("\"",""):t.optString("creator");
                i.related=!normalize(i.artist).equals(normalize(query));i.curated=i.id.startsWith("ajc");i.image="https://archive.org/services/img/"+WebRequest.encode(i.id);
                i.detail=(i.curated?"Coleção Aadam Jacobs • ":"")+(i.related?"Relacionado / tributo / menção • ":i.kind.equals("demo")?"Demo / ensaio • ":i.kind.equals("show")?"Show • ":"Gravação • ")+t.optString("date")+" • abrir faixas públicas";b.items.add(i);
            }return;
        }
        parseArchiveFiles(b,request(b,"https://archive.org/metadata/"+WebRequest.encode(route.id)),route);
    }
    static void parseArchiveFiles(Batch b,JSONObject o,Item route)throws Exception{
        JSONObject meta=o.optJSONObject("metadata");if(meta==null)throw new IOException("Metadados indisponíveis");
        if(o.optBoolean("is_dark")||meta.optBoolean("is_dark")||meta.optBoolean("access-restricted-item"))throw new IOException("Acervo restrito");
        JSONArray files=array(o,"files");TreeMap<String,JSONObject> chosen=new TreeMap<>();
        for(int n=0;n<files.length();n++){
            JSONObject t=files.getJSONObject(n);String file=t.optString("name");int priority=formatPriority(t.optString("format"));
            if(!file.toLowerCase(Locale.ROOT).matches(".*\\.(mp3|ogg|oga|flac)$")||priority==0||t.optBoolean("private")||file.isEmpty()||file.contains("..")||file.startsWith("/")||file.contains("\\"))continue;
            String original=t.optString("original",file).replaceFirst("\\.[^.]+$","");JSONObject previous=chosen.get(original);
            if(previous==null||priority>formatPriority(previous.optString("format")))chosen.put(original,t);
        }
        for(JSONObject t:chosen.values()){
            String file=t.optString("name");Item i=new Item(b.source,route.id+"/"+file,"track",name(t,"title",file));i.artist=route.artist;i.album=meta.optString("title");i.image=route.image;i.playable=true;
            i.stream="https://archive.org/download/"+WebRequest.encode(route.id)+"/"+WebRequest.encode(file).replace("+","%20");i.detail="Áudio público • "+t.optString("format");double seconds=t.optDouble("length",0);if(seconds>0&&seconds<86400)i.duration=(int)Math.round(seconds*1000);b.items.add(i);
        }
        if(b.items.isEmpty())b.error="Sem MP3, Ogg ou FLAC público neste item.";
    }
    private void jamendo(Batch b,String query,Item route,int page)throws Exception{
        String id=prefs.getString("jamendo","");if(route==null){for(String kind:new String[]{"artist","album","track"}){String endpoint="https://api.jamendo.com/v3.0/"+kind+"s/";JSONObject o=request(b,WebRequest.params(endpoint,"client_id",id,"format","json","limit","20","offset",""+page*20,kind.equals("track")?"search":"namesearch",query,"audioformat","mp32"));parseJamendo(b,o,kind,null);b.more|=array(o,"results").length()==20;}return;}
        String endpoint=route.kind.equals("artist")?"artists/tracks/":"albums/tracks/";JSONObject o=request(b,WebRequest.params("https://api.jamendo.com/v3.0/"+endpoint,"client_id",id,"format","json","id",route.id,"audioformat","mp32"));parseJamendo(b,o,"nested",route);
    }
    private void parseJamendo(Batch b,JSONObject o,String kind,Item route)throws Exception{if(!o.getJSONObject("headers").optString("status").equals("success"))throw new IOException("Credencial Jamendo recusada");JSONArray a=array(o,"results");for(int n=0;n<a.length();n++){JSONObject t=a.getJSONObject(n);if(kind.equals("nested")){JSONObject nested=new JSONObject().put("headers",o.getJSONObject("headers")).put("results",array(t,"tracks"));parseJamendo(b,nested,"track",route);continue;}Item i=new Item(b.source,t.optString("id"),kind,t.optString("name"));i.artist=name(t,"artist_name",routeArtist(route));i.image=t.optString("image",t.optString("album_image"));i.stream=t.optString("audio");i.playable=kind.equals("track")&&i.stream.startsWith("https://");i.duration=t.optInt("duration")*1000;i.genre=t.optString("genre");i.album=t.optString("album_name",t.optString("album"));i.detail="Jamendo • "+t.optString("album_name");b.items.add(i);}}
    String subUrl(String method,String... args)throws Exception{
        JSONObject c=credentials();String salt=UUID.randomUUID().toString().replace("-","");byte[] digest=MessageDigest.getInstance("MD5").digest((c.optString("password")+salt).getBytes(java.nio.charset.StandardCharsets.UTF_8));StringBuilder hex=new StringBuilder();for(byte value:digest)hex.append(String.format(Locale.ROOT,"%02x",value&255));String base=WebRequest.params(c.optString("server")+"/rest/"+method+".view","u",c.optString("user"),"s",salt,"t",hex.toString(),"v","1.16.1","c","IndieTonal","f","json");return WebRequest.params(base,args);
    }
    private void subsonic(Batch b,String query,Item route,int page)throws Exception{
        JSONObject c=credentials();if(route!=null&&(!route.server.equals(c.optString("server"))||!route.account.equals(c.optString("user"))))throw new IOException("Conexão alterada. Busque novamente.");String method=route==null?"search3":route.kind.equals("artist")?"getArtist":"getAlbum";
        String url=route==null?subUrl(method,"query",query,"artistCount","20","albumCount","20","songCount","20","artistOffset",""+page*20,"albumOffset",""+page*20,"songOffset",""+page*20):subUrl(method,"id",route.id);
        JSONObject response=request(b,url).getJSONObject("subsonic-response");if(!response.optString("status").equals("ok"))throw new IOException("Biblioteca recusou a consulta");JSONObject result=response.optJSONObject(route==null?"searchResult3":route.kind.equals("artist")?"artist":"album");if(result==null)return;
        for(String key:new String[]{"artist","album","song"}){JSONArray a=array(result,key);if(route==null)b.more|=a.length()==20;for(int n=0;n<a.length();n++){JSONObject t=a.getJSONObject(n);String kind=key.equals("song")?"track":key;Item i=new Item(b.source,t.optString("id"),kind,t.optString(kind.equals("track")?"title":"name"));i.artist=name(t,"artist",routeArtist(route));i.server=c.optString("server");i.account=c.optString("user");i.playable=kind.equals("track");i.duration=t.optInt("duration")*1000;i.genre=t.optString("genre");i.album=t.optString("album_name",t.optString("album"));i.detail="Sua biblioteca • "+t.optString("album");b.items.add(i);}}
    }
    String stream(Item item)throws Exception{if(item.source.equals("Audius"))return "https://api.audius.co/v1/tracks/"+WebRequest.encode(item.id)+"/stream?app_name=IndieTonal";if(item.source.equals("Jamendo")&&item.stream.isEmpty()){JSONObject o=WebRequest.json(WebRequest.params("https://api.jamendo.com/v3.0/tracks/","client_id",prefs.getString("jamendo",""),"id",item.id,"format","json","audioformat","mp32"));JSONArray a=array(o,"results");if(a.length()==0)throw new IOException("Faixa indisponível");item.stream=a.getJSONObject(0).optString("audio");}if(item.source.equals("OpenSubsonic")){JSONObject c=credentials();if(!item.server.equals(c.optString("server"))||!item.account.equals(c.optString("user")))throw new IOException("Conexão alterada. Busque novamente.");return subUrl("stream","id",item.id,"format","raw");}WebRequest.url(item.stream);return item.stream;}
    static String normalize(String text){return Normalizer.normalize(text,Normalizer.Form.NFD).replaceAll("\\p{M}","").toLowerCase(Locale.ROOT).replaceAll("[^\\p{L}\\p{N}]+"," ").trim();}
    static int match(String text,String query){String a=normalize(text),q=normalize(query);if(q.isEmpty()||a.isEmpty())return 0;if(a.equals(q))return 100;if(a.startsWith(q+" "))return 75;if((" "+a+" ").contains(" "+q+" "))return 65;Set<String> words=new HashSet<>(Arrays.asList(a.split(" ")));String[] terms=q.split(" ");int hits=0;for(String term:terms)if(words.contains(term))hits++;return 50*hits/terms.length;}
    static int rank(Item i,String query){int title=match(i.title,query),artist=match(i.artist,query);return i.kind.equals("artist")?title*16+(i.source.equals("MusicBrainz")?40:0)+Math.max(0,Math.min(100,i.apiScore))/5+(i.verified?20:0):Math.max(title*10,artist==100?1400:artist*9)+((i.kind.equals("show")||i.kind.equals("demo"))?35+(i.curated?25:0):i.kind.equals("album")?20:0)+(i.playable?5:0);}
    static void sort(List<Item> items,String query){items.sort(Comparator.<Item>comparingInt(i->rank(i,query)).reversed().thenComparing(Item::key));}
    public void close(){closed=true;generation++;workers.shutdownNow();}
}
