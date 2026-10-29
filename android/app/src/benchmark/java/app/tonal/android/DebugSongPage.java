package app.tonal.android;

import org.json.*;

/** Explicit synthetic screen fixture, excluded from the installed release. */
final class DebugSongPage {
    static void show(MainActivity a,SpotifyClient api){
        MusicStore store=MusicStore.get(a);if(a.getIntent().getBooleanExtra("statisticsFixture",false)){stats(a,api,store);return;}JSONObject track=new JSONObject();
        MusicStore.put(track,"provider","Spotify");MusicStore.put(track,"id","spotify:track:0123456789ABCDEFGHIJKL");MusicStore.put(track,"uri","spotify:track:0123456789ABCDEFGHIJKL");
        MusicStore.put(track,"title","Música de teste · dados sintéticos");MusicStore.put(track,"artist","Artista de teste");MusicStore.put(track,"album","Álbum de demonstração");MusicStore.put(track,"genre","grunge");MusicStore.put(track,"durationMs",180000);
        if(a.getIntent().getBooleanExtra("halloweenFixture",false)){MusicStore.put(track,"image",HalloweenTests.art());MusicStore.put(store.data,"background","Halloween");}
        store.select(track);store.lines=new JSONArray();for(int i=0;i<20;i++){JSONObject line=new JSONObject();MusicStore.put(line,"time",i*5000);MusicStore.put(line,"text","Verso de teste "+(i+1));MusicStore.put(line,"translation","Tradução sintética "+(i+1));store.lines.put(line);}store.lyricStatus="Letra e tradução sintéticas · teste de interface";store.lyricVersion++;
        MusicPanel.showSong(a,api);
    }
    private static void stats(MainActivity a,SpotifyClient api,MusicStore store){
        JSONObject track=new JSONObject();MusicStore.put(track,"provider","Spotify");MusicStore.put(track,"id","synthetic-story-0150");MusicStore.put(track,"title","Canção de demonstração");MusicStore.put(track,"artist","Artista sintético");MusicStore.put(track,"album","Álbum de teste · dados sintéticos");MusicStore.put(track,"genre","grunge, alternative rock");MusicStore.put(track,"durationMs",180000);store.select(track);
        JSONArray events=new JSONArray();for(int i=0;i<20;i++){JSONObject e=new JSONObject();MusicStore.put(e,"session","story-ui-"+i);MusicStore.put(e,"key",MusicStore.key(track));MusicStore.put(e,"at",System.currentTimeMillis()-i*3600000L);MusicStore.put(e,"ms",120000);MusicStore.put(e,"play",true);events.put(e);}MusicStore.put(store.data,"events",events);store.save();
        store.stories().capture(track,"Uma canção sobre voltar à casa da infância, recordar uma amizade distante e seguir em frente. A saudade se mistura à esperança de um novo caminho e à coragem de recomeçar.");MusicPanel.showStats(a,api);
    }
    static void setlists(MainActivity a,SpotifyClient api){SetlistWizard.show(a,api);}
}
