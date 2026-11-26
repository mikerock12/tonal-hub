package app.tonal.android;

import android.app.AlertDialog;
import android.content.Context;
import java.text.Collator;
import java.util.*;
import java.util.function.Consumer;
import java.util.function.Function;
import org.json.*;

/** Temporary track orders shared by collection pages and the play queue (0.18.1).
 * Unknown values always go last; nothing is estimated. */
final class TrackSort {
    static final String[] MODES={"original","title","duration","artist","album","genre","plays","albumFavorites","followers"};
    static final String[] LABELS={"Ordem original","Título A–Z","Duração","Artista A–Z","Álbum A–Z","Gênero A–Z","Mais tocadas (Spotify)","Favoritos do álbum","Seguidores do artista"};
    private TrackSort(){}

    static String label(String mode,boolean descending){
        for(int i=0;i<MODES.length;i++)if(MODES[i].equals(mode)){
            if(mode.equals("original"))return LABELS[i];
            String base=LABELS[i].replace(" A–Z","");
            boolean text=Arrays.asList("title","artist","album","genre").contains(mode);
            return base+(text?(descending?" Z–A":" A–Z"):(descending?" ↓":" ↑"));
        }
        return LABELS[0];
    }
    static boolean defaultDescending(String mode){return mode.equals("plays")||mode.equals("albumFavorites")||mode.equals("followers");}
    static boolean needsArtists(String mode){return mode.equals("genre")||mode.equals("followers");}
    static boolean needsPlays(String mode){return mode.equals("plays");}

    /** Single-choice menu. Choosing the current mode again inverts the direction. */
    static void choose(Context context,String title,String current,boolean descending,String[] modes,Consumer<String> chosen){choose(context,title,current,descending,modes,LABELS[0],chosen);}
    static void choose(Context context,String title,String current,boolean descending,String[] modes,String originalLabel,Consumer<String> chosen){
        String[] names=new String[modes.length];int checked=-1;
        for(int i=0;i<modes.length;i++){names[i]=modes[i].equals(current)&&!current.equals("original")?label(modes[i],descending)+" · tocar de novo inverte":modes[i].equals("original")?originalLabel:LABELS[Arrays.asList(MODES).indexOf(modes[i])];if(modes[i].equals(current))checked=i;}
        new AlertDialog.Builder(context).setTitle(title).setSingleChoiceItems(names,checked,(d,i)->{d.dismiss();chosen.accept(modes[i]);}).setNegativeButton("Fechar",null).show();
    }

    static long plays(JSONObject track,MusicStore store){
        long value=track.optLong("playCount",track.optLong("play_count",-1));
        if(value<0)value=PlayCounts.get().plays(track.optString("uri"));
        return value;
    }
    private static long metric(JSONObject track,String mode,MusicStore store){
        switch(mode){
            case "duration":{long v=track.optLong("duration_ms",-1);return v>0?v:-1;}
            case "followers":return track.optLong("artistFollowers",-1);
            case "albumFavorites":return track.optLong("albumFavoriteCount",-1);
            case "plays":{long v=plays(track,store);if(v>=0)return v;return -1;}
            default:return 0;
        }
    }
    private static String text(JSONObject track,String mode){
        switch(mode){
            case "title":return track.optString("name");
            case "artist":return CatalogScreen.artists(track);
            case "album":return MusicStore.object(track,"album").optString("name");
            case "genre":return track.optString("genre");
            default:return "";
        }
    }
    /** Stable sort; the original list is never modified. */
    static <T> ArrayList<T> sorted(List<T> input,Function<T,JSONObject> track,String mode,boolean descending,MusicStore store){
        ArrayList<T> out=new ArrayList<>(input);if(mode.equals("original"))return out;
        Collator collator=Collator.getInstance(new Locale("pt","BR"));collator.setStrength(Collator.PRIMARY);
        boolean textual=Arrays.asList("title","artist","album","genre").contains(mode);
        out.sort((x,y)->{JSONObject a=track.apply(x),b=track.apply(y);
            if(textual){String av=text(a,mode).trim(),bv=text(b,mode).trim();if(av.isEmpty()!=bv.isEmpty())return av.isEmpty()?1:-1;return (descending?-1:1)*collator.compare(av,bv);}
            long av=metric(a,mode,store),bv=metric(b,mode,store);if((av<0)!=(bv<0))return av<0?1:-1;return (descending?-1:1)*Long.compare(av,bv);});
        return out;
    }
    static String firstArtistId(JSONObject track){JSONArray artists=track.optJSONArray("artists");JSONObject a=artists!=null&&artists.length()>0?artists.optJSONObject(0):null;return a!=null?a.optString("id"):track.optString("artistId");}

    /** Fills genre/artistFollowers for the first credited artist, 50 ids per Web API call. */
    static void loadArtists(SpotifyClient api,List<JSONObject> tracks,java.util.function.BooleanSupplier alive,Runnable done,Consumer<String> failed){
        LinkedHashSet<String> wanted=new LinkedHashSet<>();for(JSONObject t:tracks){String id=firstArtistId(t);if(id.matches("[A-Za-z0-9]{22}")&&!t.optBoolean("_artistMetadata"))wanted.add(id);}
        if(wanted.isEmpty()){done.run();return;}
        loadArtists(api,tracks,new ArrayList<>(wanted),0,alive,done,failed);
    }
    private static void loadArtists(SpotifyClient api,List<JSONObject> tracks,ArrayList<String> ids,int offset,java.util.function.BooleanSupplier alive,Runnable done,Consumer<String> failed){
        String path="artists?ids="+SpotifyClient.encode(String.join(",",ids.subList(offset,Math.min(offset+50,ids.size()))));
        api.get(path,json->{if(!alive.getAsBoolean())return;
            for(Object value:MusicStore.values(MusicStore.array(json,"artists"))){if(!(value instanceof JSONObject))continue;JSONObject artist=(JSONObject)value;StringJoiner genres=new StringJoiner(", ");for(Object g:MusicStore.values(MusicStore.array(artist,"genres")))genres.add(String.valueOf(g));JSONObject followers=artist.optJSONObject("followers");
                for(JSONObject t:tracks){if(!firstArtistId(t).equals(artist.optString("id")))continue;MusicStore.put(t,"_artistMetadata",true);if(genres.length()>0||t.optString("genre").isEmpty())MusicStore.put(t,"genre",genres.toString().isEmpty()?t.optString("genre"):genres.toString());MusicStore.put(t,"artistFollowers",followers==null?-1:followers.optLong("total",-1));}}
            if(offset+50<ids.size())loadArtists(api,tracks,ids,offset+50,alive,done,failed);else done.run();
        },error->{if(alive.getAsBoolean())failed.accept(error);},alive::getAsBoolean);
    }

    /** Loads what a mode needs (artist metadata or public plays), then runs ready once. */
    static void whenReady(SpotifyClient api,List<JSONObject> tracks,String mode,java.util.function.BooleanSupplier alive,Runnable ready,Consumer<String> progress){
        if(needsArtists(mode)){progress.accept("Carregando gêneros e seguidores…");loadArtists(api,tracks,alive,ready,error->{progress.accept("Dados de artistas incompletos; itens sem dado ficam no fim.");ready.run();});return;}
        if(!needsPlays(mode)){ready.run();return;}
        PlayCounts counts=PlayCounts.get();Set<String> albums=PlayCounts.albumIds(tracks);counts.requestAlbums(albums);
        android.os.Handler main=new android.os.Handler(android.os.Looper.getMainLooper());boolean[] finished={false};Runnable[] check=new Runnable[1];
        Runnable complete=()->{if(finished[0])return;finished[0]=true;counts.forget(check[0]);if(alive.getAsBoolean())ready.run();};
        check[0]=()->{if(!alive.getAsBoolean()){finished[0]=true;counts.forget(check[0]);return;}int known=0;for(String id:albums)if(counts.albumKnown(id))known++;
            if(known>=albums.size()||counts.pendingAlbums()==0)complete.run();else progress.accept("Carregando plays do Spotify: "+known+"/"+albums.size()+" álbuns…");};
        counts.listen(check[0]);check[0].run();main.postDelayed(complete,90000);
    }
}
