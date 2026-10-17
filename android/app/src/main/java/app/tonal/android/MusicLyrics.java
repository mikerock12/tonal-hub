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
    private volatile boolean closed;
    private final LinkedHashMap<String,JSONArray> cache=new LinkedHashMap<>();
    private final HashMap<String,String> translations=new HashMap<>();
    MusicLyrics(MusicStore store){this.store=store;}
    private void result(int generation,Runnable r){main.post(()->{if(!closed&&generation==store.generation)r.run();store.lyricVersion++;});}
    void fetch(){
        if(store.busy||store.current.length()==0)return;
        String key=MusicStore.key(store.current),title=store.current.optString("title"),artist=store.current.optString("artist"),album=store.current.optString("album");int duration=store.current.optInt("durationMs"),g=++store.generation;
        if(cache.containsKey(key)){store.lyricVersion++;store.lines=cache.get(key);store.lyricStatus="LRCLIB • cache desta sessão";store.analyze();return;}
        store.busy=true;store.lyricStatus="Buscando letra no LRCLIB…";
        worker.execute(()->{try{
            String url=WebRequest.params("https://lrclib.net/api/get","track_name",title,"artist_name",artist);if(!album.isEmpty())url=WebRequest.params(url,"album_name",album);if(duration>0&&duration<3600000)url=WebRequest.params(url,"duration",String.valueOf(Math.round(duration/1000.)));
            JSONObject lyrics;
            try{lyrics=WebRequest.json(url);}catch(WebRequest.Failure e){if(e.code!=404)throw e;JSONArray all=(JSONArray)WebRequest.value(WebRequest.params("https://lrclib.net/api/search","track_name",title,"artist_name",artist));lyrics=null;double best=Double.MAX_VALUE;
                for(Object v:MusicStore.values(all)){JSONObject o=(JSONObject)v;if(!IndieCatalog.normalize(title).equals(IndieCatalog.normalize(o.optString("trackName")))||!IndieCatalog.normalize(artist).equals(IndieCatalog.normalize(o.optString("artistName"))))continue;double diff=duration>0?Math.abs(o.optDouble("duration")-duration/1000.):0;if(diff>5)continue;double score=diff+(o.optString("syncedLyrics").isEmpty()?100:0);if(score<best){lyrics=o;best=score;}}
            }
            JSONObject found=lyrics;result(g,()->{store.busy=false;if(found==null){store.lyricStatus="Letra não encontrada para esta versão. Importe TXT/LRC.";return;}String synced=found.optString("syncedLyrics");if(synced.equals("null"))synced="";String plain=found.optString("plainLyrics");if(plain.equals("null"))plain="";store.lines=MusicStore.parseLyrics(synced.isEmpty()?plain:synced);store.lyricStatus=found.optBoolean("instrumental")?"Instrumental • LRCLIB":synced.isEmpty()?"LRCLIB • rolagem manual":"LRCLIB • letra sincronizada";if(cache.size()>=30)cache.remove(cache.keySet().iterator().next());cache.put(key,store.lines);store.analyze();});
        }catch(Exception error){result(g,()->{store.busy=false;store.lyricStatus="Letra indisponível agora. Tente novamente ou importe TXT/LRC.";});}});
    }
    void translate(String lang){
        if(store.busy||store.lines.length()==0)return;if(lang.equals("pt")){store.lyricStatus="A letra já está em português.";return;}
        if(!Arrays.asList("en","es","fr","de","it","ja","ko").contains(lang))return;
        store.busy=true;store.lyricStatus="Traduzindo pt-BR • Google Tradutor";int g=++store.generation;translateLine(lang,0,g);
    }
    private void translateLine(String lang,int index,int g){
        if(closed||g!=store.generation)return;
        if(index>=store.lines.length()){store.busy=false;store.lyricStatus="LRCLIB + Google Tradutor • tradução automática pt-BR, pode conter erros";return;}
        JSONObject row=store.lines.optJSONObject(index);String text=row.optString("text"),k=lang+"|"+text;
        if(text.isEmpty()||translations.containsKey(k)){if(!text.isEmpty())MusicStore.put(row,"translation",translations.get(k));main.post(()->translateLine(lang,index+1,g));return;}
        String day=new java.text.SimpleDateFormat("yyyy-MM-dd",Locale.ROOT).format(new Date());if(!day.equals(store.data.optString("translationDay"))){MusicStore.put(store.data,"translationDay",day);MusicStore.put(store.data,"translationBytes",0);}int bytes=text.getBytes(StandardCharsets.UTF_8).length,used=store.data.optInt("translationBytes");
        if(bytes>480||used+bytes>50000){store.busy=false;store.lyricStatus="Tradução parcial: limite local diário ou linha longa. Importe uma tradução.";return;}
        MusicStore.put(store.data,"translationBytes",used+bytes);store.save();
        worker.execute(()->{try{JSONArray o=(JSONArray)WebRequest.value(WebRequest.params("https://translate.googleapis.com/translate_a/single","client","gtx","sl",lang,"tl","pt-BR","dt","t","q",text));StringBuilder result=new StringBuilder();JSONArray segments=o.optJSONArray(0);if(segments!=null)for(Object segment:MusicStore.values(segments))if(segment instanceof JSONArray)result.append(((JSONArray)segment).optString(0));String translated=result.toString();if(translated.isEmpty())throw new Exception();result(g,()->{if(translations.size()>=1000)translations.clear();translations.put(k,translated);MusicStore.put(row,"translation",translated);main.postDelayed(()->translateLine(lang,index+1,g),350);});}catch(Exception e){result(g,()->{store.busy=false;store.lyricStatus="Tradução parcial/indisponível • cota ou falha do Google Tradutor. Tente mais tarde.";});}});
    }
    public void close(){closed=true;worker.shutdownNow();main.removeCallbacksAndMessages(null);store.busy=false;}
}
