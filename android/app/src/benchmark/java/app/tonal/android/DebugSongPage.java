package app.tonal.android;

import org.json.*;

/** Explicit synthetic screen fixture, excluded from the installed release. */
final class DebugSongPage {
    static void show(MainActivity a,SpotifyClient api){
        MusicStore store=MusicStore.get(a);JSONObject track=new JSONObject();
        MusicStore.put(track,"provider","Spotify");MusicStore.put(track,"id","spotify:track:0123456789ABCDEFGHIJKL");MusicStore.put(track,"uri","spotify:track:0123456789ABCDEFGHIJKL");
        MusicStore.put(track,"title","Música de teste · dados sintéticos");MusicStore.put(track,"artist","Artista de teste");MusicStore.put(track,"album","Álbum de demonstração");MusicStore.put(track,"genre","grunge");MusicStore.put(track,"durationMs",180000);
        store.select(track);store.lines=new JSONArray();for(int i=0;i<20;i++){JSONObject line=new JSONObject();MusicStore.put(line,"time",i*5000);MusicStore.put(line,"text","Verso de teste "+(i+1));MusicStore.put(line,"translation","Tradução sintética "+(i+1));store.lines.put(line);}store.lyricStatus="Letra e tradução sintéticas · teste de interface";store.lyricVersion++;
        MusicPanel.showSong(a,api);
    }
    static void setlists(MainActivity a,SpotifyClient api){SetlistWizard.show(a,api);}
}
