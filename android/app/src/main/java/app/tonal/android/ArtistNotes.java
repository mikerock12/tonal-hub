package app.tonal.android;

import android.content.Context;
import android.os.*;
import org.json.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** On-demand facts, not generated biographies (0.18.1).
 * Identity: the Spotify artist id registered in MusicBrainz (exact), else a unique name/album
 * match; homonyms are never guessed. Members/ages come from MusicBrainz relations and life spans.
 * Bounded cache/requests (shared 1,25 s throttle), cancellable delivery, attributed source. */
final class ArtistNotes implements AutoCloseable {
    private static final int MAX_MEMBERS=8,MAX_MEMBER_LOOKUPS=6;
    private final Context context;private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final Handler main=new Handler(Looper.getMainLooper());private volatile boolean closed;private volatile int generation;
    ArtistNotes(Context c){context=c.getApplicationContext();}

    /** ready may run twice: facts first, then the same result with member details. */
    void load(JSONObject track,Consumer<JSONObject> ready){
        final int request=++generation;final String name=track.optString("artist"),genre=track.optString("genre"),artistId=track.optString("artistId"),
            album=track.optString("album").replaceAll("\\s*\\(.*?\\)|\\s*\\[.*?\\]", "").trim(),identity=artistId.matches("[A-Za-z0-9]{22}")?"spotify:"+artistId:name+"|"+album;
        worker.execute(()->{JSONObject result=new JSONObject();File file=new File(context.getCacheDir(),"artist-notes-v2.json");JSONObject cache=new JSONObject();
            try{if(file.isFile()&&file.length()<1024*1024)cache=new JSONObject(new String(java.nio.file.Files.readAllBytes(file.toPath()),java.nio.charset.StandardCharsets.UTF_8));JSONObject saved=cache.optJSONObject(identity);
                if(saved!=null&&System.currentTimeMillis()-saved.optLong("at")<saved.optLong("ttl"))result=saved;
                else {
                    String id=artistId.matches("[A-Za-z0-9]{22}")?fromSpotifyId(artistId):"";
                    if(id.isEmpty()){JSONObject match=MusicMetadata.uniqueArtist(WebRequest.musicBrainz("artist/?fmt=json&limit=8&query="+SpotifyClient.encode("artist:\""+name.replace("\"","")+"\"")),name);
                        if(match!=null)id=match.optString("id");
                        else if(!album.isEmpty())id=artistFromRelease(WebRequest.musicBrainz("release-group/?fmt=json&limit=10&query="+SpotifyClient.encode("artist:\""+name.replace("\"","")+"\" AND releasegroup:\""+album.replace("\"","")+"\"")),name,album);}
                    if(id.isEmpty())MusicStore.put(result,"message","A fonte não identificou um único artista com segurança. Nenhum fato foi atribuído a homônimos.");
                    else {result=facts(WebRequest.musicBrainz("artist/"+id+"?fmt=json&inc=genres+tags+artist-rels"));deliver(request,result,name,genre,ready);if(request!=generation)return;result=withMemberDetails(result,request);}
                    MusicStore.put(result,"at",System.currentTimeMillis());MusicStore.put(result,"ttl",result.optString("url").isEmpty()?3600000:7L*86400000);MusicStore.put(cache,identity,result);while(cache.length()>32)cache.remove(cache.keys().next());java.nio.file.Files.write(file.toPath(),cache.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));}
            }catch(Exception e){if(!result.has("facts")){result=new JSONObject();MusicStore.put(result,"message",e instanceof WebRequest.Failure&&(((WebRequest.Failure)e).code==503||((WebRequest.Failure)e).code==429)?"O MusicBrainz está sobrecarregado agora. Tente de novo em instantes; sua música continua tocando.":"Informações do artista temporariamente indisponíveis. Sua música continua tocando.");MusicStore.put(result,"retry",true);}}
            deliver(request,result,name,genre,ready);
        });
    }
    private void deliver(int request,JSONObject result,String name,String genre,Consumer<JSONObject> ready){
        String relatedGenre=result.optString("genre");MusicStore.put(result,"related",related(context,name,relatedGenre.isEmpty()?genre:relatedGenre));
        JSONObject value;try{value=new JSONObject(result.toString());}catch(JSONException e){value=result;}JSONObject copy=value;
        main.post(()->{if(!closed&&request==generation)ready.accept(copy);});
    }

    /** Exact identity: MusicBrainz URL relationship for open.spotify.com/artist/{id}. */
    static String fromSpotifyId(String spotifyId)throws Exception{
        try{return uniqueLinkedArtist(WebRequest.musicBrainz("url?fmt=json&inc=artist-rels&resource="+SpotifyClient.encode("https://open.spotify.com/artist/"+spotifyId)));}
        catch(WebRequest.Failure e){if(e.code==404)return "";throw e;}
    }
    static String uniqueLinkedArtist(JSONObject url){Set<String> ids=new HashSet<>();for(Object v:MusicStore.values(MusicStore.array(url,"relations"))){JSONObject rel=(JSONObject)v;if(!rel.optString("target-type").equals("artist"))continue;String id=MusicStore.object(rel,"artist").optString("id");if(id.matches("[a-fA-F0-9-]{36}"))ids.add(id);}return ids.size()==1?ids.iterator().next():"";}

    static String artistFromRelease(JSONObject response,String name,String album){Set<String> ids=new HashSet<>();for(Object value:MusicStore.values(MusicStore.array(response,"release-groups"))){JSONObject release=(JSONObject)value;if(!IndieCatalog.normalize(album).equals(IndieCatalog.normalize(release.optString("title"))))continue;for(Object credit:MusicStore.values(MusicStore.array(release,"artist-credit"))){JSONObject artist=MusicStore.object((JSONObject)credit,"artist");if(IndieCatalog.normalize(name).equals(IndieCatalog.normalize(artist.optString("name")))&&!artist.optString("id").isEmpty())ids.add(artist.optString("id"));}}return ids.size()==1?ids.iterator().next():"";}

    static JSONObject facts(JSONObject artist){JSONObject out=new JSONObject();JSONArray facts=new JSONArray();boolean group=artist.optString("type").equals("Group");
        String country=artist.optString("country"),begin=MusicStore.object(artist,"life-span").optString("begin"),end=MusicStore.object(artist,"life-span").optString("end"),place=MusicStore.object(artist,"begin-area").optString("name"),area=MusicStore.object(artist,"area").optString("name");
        if(!place.isEmpty())facts.put("Origem: "+place+(country.isEmpty()?"":", "+country));else if(!area.isEmpty())facts.put("Origem: "+area);else if(!country.isEmpty())facts.put("País: "+country);
        if(group){if(!begin.isEmpty())facts.put("Início: "+year(begin)+(end.isEmpty()?(MusicStore.object(artist,"life-span").optBoolean("ended")?"":" · em atividade"):" · encerrada em "+year(end)));}
        else if(!begin.isEmpty())facts.put("Nascimento: "+begin+age(begin,end));
        String genre=MusicMetadata.genres(artist);if(!genre.isEmpty())facts.put("Estilos: "+genre);String info=artist.optString("disambiguation");if(!info.isEmpty())facts.put(info);
        // Members of a group: relations pointing from the person to this band ("backward").
        LinkedHashMap<String,JSONObject> members=new LinkedHashMap<>();
        for(Object value:MusicStore.values(MusicStore.array(artist,"relations"))){JSONObject rel=(JSONObject)value;if(!rel.optString("type").equals("member of band"))continue;JSONObject person=MusicStore.object(rel,"artist");String id=person.optString("id"),name=person.optString("name");if(name.isEmpty())continue;
            if(group&&!rel.optString("direction").equals("backward"))continue;String key=id.isEmpty()?name:id;JSONObject m=members.get(key);if(m==null){if(members.size()>=24)continue;m=new JSONObject();MusicStore.put(m,"id",id);MusicStore.put(m,"name",name);MusicStore.put(m,"roles",new JSONArray());MusicStore.put(m,"current",false);MusicStore.put(m,"original",false);MusicStore.put(m,"from","");MusicStore.put(m,"to","");members.put(key,m);}
            for(Object a:MusicStore.values(MusicStore.array(rel,"attributes"))){String role=a.toString();if(role.equals("original")){MusicStore.put(m,"original",true);continue;}role=role(role);JSONArray roles=MusicStore.array(m,"roles");boolean seen=false;for(Object r:MusicStore.values(roles))if(r.toString().equals(role))seen=true;if(!seen&&roles.length()<4)roles.put(role);}
            if(!rel.optBoolean("ended"))MusicStore.put(m,"current",true);
            String from=rel.optString("begin",""),to=rel.optString("end","");if(from.equals("null"))from="";if(to.equals("null"))to="";
            if(!from.isEmpty()&&(m.optString("from").isEmpty()||from.compareTo(m.optString("from"))<0))MusicStore.put(m,"from",from);
            if(!to.isEmpty()&&to.compareTo(m.optString("to"))>0)MusicStore.put(m,"to",to);}
        ArrayList<JSONObject> ordered=new ArrayList<>(members.values());
        ordered.sort((a,b)->{int c=Boolean.compare(b.optBoolean("current"),a.optBoolean("current"));if(c!=0)return c;c=Boolean.compare(b.optBoolean("original"),a.optBoolean("original"));if(c!=0)return c;return a.optString("from","9999").compareTo(b.optString("from","9999"));});
        JSONArray list=new JSONArray();for(JSONObject m:ordered)if(list.length()<MAX_MEMBERS)list.put(m);
        MusicStore.put(out,"members",list);MusicStore.put(out,"memberTotal",ordered.size());MusicStore.put(out,"group",group);
        MusicStore.put(out,"facts",facts);MusicStore.put(out,"genre",genre);String id=artist.optString("id");if(id.matches("[a-fA-F0-9-]{36}"))MusicStore.put(out,"url","https://musicbrainz.org/artist/"+id);return out;
    }

    /** Birth/death years of up to six listed members; failures keep the list without ages. */
    private JSONObject withMemberDetails(JSONObject result,int request){
        JSONArray members=MusicStore.array(result,"members");int lookups=0;
        for(int i=0;i<members.length()&&lookups<MAX_MEMBER_LOOKUPS;i++){if(closed||request!=generation)break;JSONObject m=members.optJSONObject(i);String id=m==null?"":m.optString("id");if(!id.matches("[a-fA-F0-9-]{36}"))continue;lookups++;
            try{JSONObject person=WebRequest.musicBrainz("artist/"+id+"?fmt=json");JSONObject life=MusicStore.object(person,"life-span");MusicStore.put(m,"born",life.optString("begin").equals("null")?"":life.optString("begin"));MusicStore.put(m,"died",life.optString("end").equals("null")?"":life.optString("end"));MusicStore.put(m,"deceased",life.optBoolean("ended"));}
            catch(Exception e){break;}}
        return result;
    }

    /** One line per member: name, roles, period and age when MusicBrainz has the dates. */
    static String memberLine(JSONObject m){
        StringBuilder line=new StringBuilder(m.optString("name"));JSONArray roles=MusicStore.array(m,"roles");
        if(roles.length()>0){StringJoiner r=new StringJoiner(", ");for(Object v:MusicStore.values(roles))r.add(v.toString());line.append(" — ").append(r);}
        String from=year(m.optString("from")),to=year(m.optString("to"));
        if(m.optBoolean("current"))line.append(from.isEmpty()?" · atual":" · desde "+from);else if(!from.isEmpty()||!to.isEmpty())line.append(" · ").append(from.isEmpty()?"?":from).append("–").append(to.isEmpty()?"?":to);
        String born=m.optString("born"),died=m.optString("died");
        if(!born.isEmpty()){if(!died.isEmpty())line.append(" · ").append(year(born)).append("–").append(year(died)).append(age(born,died));else if(!m.optBoolean("deceased"))line.append(" · ").append(age(born,"").replace(" (","").replace(")",""));}
        return line.toString();
    }
    static String year(String date){return CatalogFormat.year(date);}
    static String age(String born,String died){return CatalogFormat.age(born,died,java.util.Calendar.getInstance());}
    private static String role(String value){
        switch(value){case "lead vocals":return "vocal";case "background vocals":case "other vocals":return "vocal de apoio";case "guitar":return "guitarra";case "bass guitar":case "bass":return "baixo";
            case "drums (drum set)":case "drums":return "bateria";case "keyboard":case "keyboards":return "teclado";case "piano":return "piano";case "rhythm guitar":return "guitarra base";case "lead guitar":return "guitarra solo";
            case "electric guitar":return "guitarra";case "acoustic guitar":return "violão";case "percussion":return "percussão";case "synthesizer":return "sintetizador";case "violin":return "violino";case "saxophone":return "saxofone";case "trumpet":return "trompete";
            default:return value;}
    }
    static JSONArray related(Context context,String artist,String genres){JSONArray out=new JSONArray();try(InputStream input=context.getAssets().open("music-rules.json")){JSONObject rules=new JSONObject(readText(input));Set<String> seen=new HashSet<>();for(Object value:MusicStore.values(MusicStore.array(rules,"recommendations"))){JSONObject rule=(JSONObject)value;boolean match=false;for(Object a:MusicStore.values(MusicStore.array(rule,"artists")))if(a.toString().equalsIgnoreCase(artist))match=true;for(Object g:MusicStore.values(MusicStore.array(rule,"genres")))if(genres.toLowerCase(Locale.ROOT).contains(g.toString().toLowerCase(Locale.ROOT)))match=true;if(match)for(Object a:MusicStore.values(MusicStore.array(rule,"artists")))if(!a.toString().equalsIgnoreCase(artist)&&seen.add(a.toString())&&out.length()<8)out.put(a);}}catch(Exception ignored){}return out;}
    private static String readText(InputStream in)throws IOException{ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] bytes=new byte[4096];for(int n;(n=in.read(bytes))!=-1;){if(out.size()+n>128*1024)throw new IOException();out.write(bytes,0,n);}return new String(out.toByteArray(),java.nio.charset.StandardCharsets.UTF_8);}
    public void close(){closed=true;++generation;worker.shutdownNow();main.removeCallbacksAndMessages(null);}
}
