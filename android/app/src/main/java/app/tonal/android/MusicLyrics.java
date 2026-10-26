package app.tonal.android;

import android.os.*;
import org.json.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Explicit requests only. Never logs lyric text or translation query URLs. */
final class MusicLyrics implements AutoCloseable {
    private final MusicStore store;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final Handler main=new Handler(Looper.getMainLooper());
    private volatile boolean closed;private String retryKey="";private int retries;
    private final LinkedHashMap<String,JSONArray> cache=new LinkedHashMap<>();
    private final HashMap<String,String> translations=new HashMap<>();
    interface Translator{String translate(String language,String text)throws Exception;}
    private final Translator translator;
    interface Fetcher{Object value(String url)throws Exception;}
    private final Fetcher fetcher;
    MusicLyrics(MusicStore store){this(store,null);}
    MusicLyrics(MusicStore store,Translator translator){this(store,translator,WebRequest::value);}
    MusicLyrics(MusicStore store,Translator translator,Fetcher fetcher){this.store=store;this.translator=translator;this.fetcher=fetcher;}
    private void result(int generation,Runnable r){main.post(()->{if(!closed&&generation==store.generation){r.run();store.lyricVersion++;}});}
    void fetch(){if(store.busy)return;retries=0;fetchAttempt();}
    void refresh(){if(store.busy)return;cache.remove(MusicStore.key(store.current));fetch();}
    private void fetchAttempt(){
        if(store.busy||store.current.length()==0)return;
        String key=MusicStore.key(store.current),title=store.current.optString("title"),artist=store.current.optString("artist"),album=store.current.optString("album");int duration=store.current.optInt("durationMs"),g=++store.generation;if(!key.equals(retryKey)){retryKey=key;retries=0;}
        if(cache.containsKey(key)){store.lyricVersion++;store.lines=cache.get(key);store.lyricStatus="LRCLIB • cache desta sessão";store.analyze();return;}
        store.busy=true;store.lyricStatus="Buscando letra no LRCLIB…";
        worker.execute(()->{try{
            String url=WebRequest.params("https://lrclib.net/api/get","track_name",title,"artist_name",artist);if(!album.isEmpty())url=WebRequest.params(url,"album_name",album);if(duration>0&&duration<3600000)url=WebRequest.params(url,"duration",String.valueOf(Math.round(duration/1000.)));
            JSONObject lyrics;
            try{lyrics=(JSONObject)fetcher.value(url);}catch(WebRequest.Failure e){if(e.code!=404&&!(duration<=0&&e.code==400)&&!(retries>=2&&Arrays.asList(408,500,502,503,504).contains(e.code)))throw e;JSONArray all=(JSONArray)fetcher.value(WebRequest.params("https://lrclib.net/api/search","track_name",title,"artist_name",artist));lyrics=null;double best=Double.MAX_VALUE;
                for(Object v:MusicStore.values(all)){JSONObject o=(JSONObject)v;if(!IndieCatalog.normalize(title).equals(IndieCatalog.normalize(o.optString("trackName")))||!IndieCatalog.normalize(artist).equals(IndieCatalog.normalize(o.optString("artistName"))))continue;double diff=duration>0?Math.abs(o.optDouble("duration")-duration/1000.):0;if(diff>5)continue;double score=diff+(o.optString("syncedLyrics").isEmpty()?100:0);if(score<best){lyrics=o;best=score;}}
            }
            JSONObject found=lyrics;result(g,()->{store.busy=false;retries=0;if(found==null){store.lyricStatus="Letra não encontrada para esta versão. Importe TXT/LRC.";return;}String synced=found.optString("syncedLyrics");if(synced.equals("null"))synced="";String plain=found.optString("plainLyrics");if(plain.equals("null"))plain="";store.lines=MusicStore.parseLyrics(synced.isEmpty()?plain:synced);store.lyricStatus=found.optBoolean("instrumental")?"Instrumental • LRCLIB":synced.isEmpty()?"LRCLIB • rolagem manual":"LRCLIB • letra sincronizada";if(cache.size()>=30)cache.remove(cache.keySet().iterator().next());cache.put(key,store.lines);store.analyze();});
        }catch(Exception error){result(g,()->{store.busy=false;if((!(error instanceof WebRequest.Failure)||Arrays.asList(408,429,500,502,503,504).contains(((WebRequest.Failure)error).code))&&retries<2){int delay=1500*(1<<retries++);if(error instanceof WebRequest.Failure)try{delay=Math.max(delay,Math.min(60000,Integer.parseInt(((WebRequest.Failure)error).retryAfter)*1000));}catch(NumberFormatException ignored){}store.busy=true;store.lyricStatus="Serviço de letras ocupado • nova tentativa em "+(delay/1000)+" s…";main.postDelayed(()->{if(!closed&&g==store.generation){store.busy=false;fetchAttempt();}},delay);return;}store.lyricStatus="O serviço de letras está temporariamente indisponível"+(error instanceof WebRequest.Failure?" (HTTP "+((WebRequest.Failure)error).code+")":"")+". Tente novamente ou importe TXT/LRC.";});}});
    }
    void cancel(){store.generation++;store.busy=false;main.removeCallbacksAndMessages(null);}
    void translate(String lang){
        if(store.busy||store.lines.length()==0)return;if(lang.equals("pt")){store.lyricStatus="A letra já está em português.";return;}
        if(!Arrays.asList("auto","en","es","fr","de","it","ja","ko").contains(lang))return;
        failures=0;store.busy=true;store.lyricStatus="Traduzindo pt-BR • Google Tradutor";int g=++store.generation;translateLine(lang,0,g);
    }
    private int failures;
    private void translateLine(String lang,int index,int g){translateLine(lang,index,g,0);}
    private void translateLine(String lang,int index,int g,int attempt){
        if(closed||g!=store.generation)return;
        if(index>=store.lines.length()){store.busy=false;store.lyricVersion++;int complete=0,eligible=0;for(Object v:MusicStore.values(store.lines)){JSONObject row=(JSONObject)v;if(!row.optString("text").isEmpty()){eligible++;if(!row.optString("translation").isEmpty())complete++;}}store.lyricStatus="Tradução pt-BR • "+complete+"/"+eligible+" linhas"+(failures>0?" • toque em traduzir para retomar pendências":" • Google Tradutor, pode conter erros");return;}
        JSONObject row=store.lines.optJSONObject(index);String text=row.optString("text"),k=lang+"|"+text;
        if(text.isEmpty()||!row.optString("translation").isEmpty()||translations.containsKey(k)){if(!text.isEmpty()&&translations.containsKey(k))MusicStore.put(row,"translation",translations.get(k));store.lyricVersion++;main.post(()->translateLine(lang,index+1,g));return;}
        String day=new java.text.SimpleDateFormat("yyyy-MM-dd",Locale.ROOT).format(new Date());if(!day.equals(store.data.optString("translationDay"))){MusicStore.put(store.data,"translationDay",day);MusicStore.put(store.data,"translationBytes",0);}int bytes=text.getBytes(StandardCharsets.UTF_8).length,used=store.data.optInt("translationBytes");
        if(used+bytes>512*1024){store.busy=false;store.lyricVersion++;store.lyricStatus="Tradução parcial • proteção local diária de 512 KiB atingida. Retome amanhã ou importe tradução.";return;}
        MusicStore.put(store.data,"translationBytes",used+bytes);store.save();store.lyricStatus="Traduzindo pt-BR • linha "+(index+1)+"/"+store.lines.length();
        worker.execute(()->{try{StringBuilder translated=new StringBuilder();if(translator!=null)translated.append(translator.translate(lang,text));else for(String part:chunks(text)){
            JSONArray o=(JSONArray)WebRequest.value(WebRequest.params("https://translate.googleapis.com/translate_a/single","client","gtx","sl",lang,"tl","pt-BR","dt","t","q",part));StringBuilder result=new StringBuilder();JSONArray segments=o.optJSONArray(0);if(segments!=null)for(Object segment:MusicStore.values(segments))if(segment instanceof JSONArray)result.append(((JSONArray)segment).optString(0));if(result.length()==0)throw new java.io.IOException();if(translated.length()>0)translated.append(' ');translated.append(result);
        }String value=translated.toString();result(g,()->{if(translations.size()>=2000)translations.remove(translations.keySet().iterator().next());translations.put(k,value);MusicStore.put(row,"translation",value);main.postDelayed(()->translateLine(lang,index+1,g),350);});}catch(Exception e){result(g,()->{boolean transientFailure=!(e instanceof WebRequest.Failure)||Arrays.asList(408,429,500,502,503,504).contains(((WebRequest.Failure)e).code);if(transientFailure&&attempt<2){store.lyricStatus="Retomando tradução • linha "+(index+1);main.postDelayed(()->translateLine(lang,index,g,attempt+1),1500L*(attempt+1));}else{failures++;main.postDelayed(()->translateLine(lang,index+1,g),750);}});}});
    }
    static List<String> chunks(String text){ArrayList<String> parts=new ArrayList<>();StringBuilder part=new StringBuilder();for(int i=0;i<text.length();){int cp=text.codePointAt(i);String c=new String(Character.toChars(cp));if((part.toString()+c).getBytes(StandardCharsets.UTF_8).length>440){parts.add(part.toString());part.setLength(0);}part.append(c);i+=Character.charCount(cp);}if(part.length()>0)parts.add(part.toString());return parts;}
    public void close(){closed=true;worker.shutdownNow();main.removeCallbacksAndMessages(null);store.busy=false;}
}
