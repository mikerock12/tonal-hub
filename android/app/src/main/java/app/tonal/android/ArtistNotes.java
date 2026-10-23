package app.tonal.android;

import android.content.Context;
import android.os.*;
import org.json.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** On-demand facts, not generated biographies. Exact unique MusicBrainz match;
 * bounded cache/requests, cancellable delivery and attributed editorial links. */
final class ArtistNotes implements AutoCloseable {
    private final Context context;private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final Handler main=new Handler(Looper.getMainLooper());private volatile boolean closed;private int generation;
    ArtistNotes(Context c){context=c.getApplicationContext();}
    void load(JSONObject track,Consumer<JSONObject> ready){
        final int request=++generation;final String name=track.optString("artist"),genre=track.optString("genre"),album=track.optString("album").replaceAll("\\s*\\(.*?\\)|\\s*\\[.*?\\]", "").trim(),identity=name+"|"+album;
        worker.execute(()->{JSONObject result=new JSONObject();File file=new File(context.getCacheDir(),"artist-notes-v1.json");JSONObject cache=new JSONObject();
            try{if(file.isFile()&&file.length()<1024*1024)cache=new JSONObject(new String(java.nio.file.Files.readAllBytes(file.toPath()),java.nio.charset.StandardCharsets.UTF_8));JSONObject saved=cache.optJSONObject(identity);
                if(saved!=null&&System.currentTimeMillis()-saved.optLong("at")<saved.optLong("ttl"))result=saved;
                else {JSONObject response=WebRequest.musicBrainz("artist/?fmt=json&limit=8&query="+SpotifyClient.encode("artist:\""+name.replace("\"","")+"\""));JSONObject match=MusicMetadata.uniqueArtist(response,name);if(match==null&&!album.isEmpty()){String id=artistFromRelease(WebRequest.musicBrainz("release-group/?fmt=json&limit=10&query="+SpotifyClient.encode("artist:\""+name.replace("\"","")+"\" AND releasegroup:\""+album.replace("\"","")+"\"")),name,album);if(!id.isEmpty()){match=new JSONObject();MusicStore.put(match,"id",id);}}
                    if(match==null)MusicStore.put(result,"message","A fonte não identificou um único artista com segurança. Nenhum fato foi atribuído a homônimos.");
                    else result=facts(WebRequest.musicBrainz("artist/"+match.optString("id")+"?fmt=json&inc=genres+tags+artist-rels"));
                    MusicStore.put(result,"at",System.currentTimeMillis());MusicStore.put(result,"ttl",result.optString("url").isEmpty()?3600000:7L*86400000);MusicStore.put(cache,identity,result);while(cache.length()>32)cache.remove(cache.keys().next());java.nio.file.Files.write(file.toPath(),cache.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));}
            }catch(Exception e){MusicStore.put(result,"message","Informações do artista temporariamente indisponíveis. Sua música continua tocando.");}
            String relatedGenre=result.optString("genre");MusicStore.put(result,"related",related(context,name,relatedGenre.isEmpty()?genre:relatedGenre));JSONObject value=result;
            main.post(()->{if(!closed&&request==generation)ready.accept(value);});
        });
    }
    static String artistFromRelease(JSONObject response,String name,String album){Set<String> ids=new HashSet<>();for(Object value:MusicStore.values(MusicStore.array(response,"release-groups"))){JSONObject release=(JSONObject)value;if(!IndieCatalog.normalize(album).equals(IndieCatalog.normalize(release.optString("title"))))continue;for(Object credit:MusicStore.values(MusicStore.array(release,"artist-credit"))){JSONObject artist=MusicStore.object((JSONObject)credit,"artist");if(IndieCatalog.normalize(name).equals(IndieCatalog.normalize(artist.optString("name")))&&!artist.optString("id").isEmpty())ids.add(artist.optString("id"));}}return ids.size()==1?ids.iterator().next():"";}
    static JSONObject facts(JSONObject artist){JSONObject out=new JSONObject();JSONArray facts=new JSONArray();String country=artist.optString("country"),begin=MusicStore.object(artist,"life-span").optString("begin"),place=MusicStore.object(artist,"begin-area").optString("name");
        if(!place.isEmpty())facts.put("Origem: "+place+(country.isEmpty()?"":" · "+country));else if(!country.isEmpty())facts.put("País: "+country);
        if(!begin.isEmpty())facts.put((artist.optString("type").equals("Group")?"Formação: ":"Nascimento: ")+begin);
        String end=MusicStore.object(artist,"life-span").optString("end");if(!end.isEmpty())facts.put("Fim do período registrado: "+end);
        String genre=MusicMetadata.genres(artist);if(!genre.isEmpty())facts.put("Estilos: "+genre);String info=artist.optString("disambiguation");if(!info.isEmpty())facts.put(info);
        LinkedHashSet<String> members=new LinkedHashSet<>();for(Object value:MusicStore.values(MusicStore.array(artist,"relations"))){JSONObject rel=(JSONObject)value;if(rel.optString("type").equals("member of band")){String name=MusicStore.object(rel,"artist").optString("name");if(!name.isEmpty()&&members.size()<6)members.add(name);}}
        if(!members.isEmpty())facts.put("Conexões / integrantes registrados: "+String.join(", ",members));
        MusicStore.put(out,"facts",facts);MusicStore.put(out,"genre",genre);String id=artist.optString("id");if(id.matches("[a-fA-F0-9-]{36}"))MusicStore.put(out,"url","https://musicbrainz.org/artist/"+id);return out;
    }
    static JSONArray related(Context context,String artist,String genres){JSONArray out=new JSONArray();try(InputStream input=context.getAssets().open("music-rules.json")){JSONObject rules=new JSONObject(readText(input));Set<String> seen=new HashSet<>();for(Object value:MusicStore.values(MusicStore.array(rules,"recommendations"))){JSONObject rule=(JSONObject)value;boolean match=false;for(Object a:MusicStore.values(MusicStore.array(rule,"artists")))if(a.toString().equalsIgnoreCase(artist))match=true;for(Object g:MusicStore.values(MusicStore.array(rule,"genres")))if(genres.toLowerCase(Locale.ROOT).contains(g.toString().toLowerCase(Locale.ROOT)))match=true;if(match)for(Object a:MusicStore.values(MusicStore.array(rule,"artists")))if(!a.toString().equalsIgnoreCase(artist)&&seen.add(a.toString())&&out.length()<8)out.put(a);}}catch(Exception ignored){}return out;}
    private static String readText(InputStream in)throws IOException{ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] bytes=new byte[4096];for(int n;(n=in.read(bytes))!=-1;){if(out.size()+n>128*1024)throw new IOException();out.write(bytes,0,n);}return new String(out.toByteArray(),java.nio.charset.StandardCharsets.UTF_8);}
    public void close(){closed=true;++generation;worker.shutdownNow();main.removeCallbacksAndMessages(null);}
}
