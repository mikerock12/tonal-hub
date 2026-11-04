package app.tonal.android;

import org.json.*;

/** Explicit synthetic screen fixture, excluded from the installed release. */
final class DebugSongPage {
    static void show(MainActivity a,SpotifyClient api){
        MusicStore store=MusicStore.get(a);if(a.getIntent().getBooleanExtra("statisticsFixture",false)){stats(a,api,store);return;}JSONObject track=new JSONObject();
        MusicStore.put(track,"provider","Spotify");MusicStore.put(track,"id","spotify:track:0123456789ABCDEFGHIJKL");MusicStore.put(track,"uri","spotify:track:0123456789ABCDEFGHIJKL");
        MusicStore.put(track,"title","Música de teste · dados sintéticos");MusicStore.put(track,"artist","Artista de teste");MusicStore.put(track,"album","Álbum de demonstração");MusicStore.put(track,"genre","grunge");MusicStore.put(track,"durationMs",180000);
        String fixtureBackground=a.getIntent().getStringExtra("fixtureBackground");if(fixtureBackground!=null&&!fixtureBackground.isEmpty()){MusicStore.put(track,"image",HalloweenTests.art());MusicStore.put(store.data,"background",fixtureBackground);}else if(a.getIntent().getBooleanExtra("halloweenFixture",false)){MusicStore.put(track,"image",HalloweenTests.art());MusicStore.put(store.data,"background","Halloween");}
        store.select(track);store.lines=new JSONArray();for(int i=0;i<20;i++){JSONObject line=new JSONObject();MusicStore.put(line,"time",i*5000);MusicStore.put(line,"text","Verso de teste "+(i+1));MusicStore.put(line,"translation","Tradução sintética "+(i+1));store.lines.put(line);}store.lyricStatus="Letra e tradução sintéticas · teste de interface";store.lyricVersion++;
        if(a.getIntent().getBooleanExtra("fixtureAudio",false))tone(a);
        MusicPanel.showSong(a,api);
    }
    /** Real AudioTrack load for measurements: a synthetic tone, reported in logcat. */
    private static void tone(MainActivity a){
        NativeBridge.audio.initialize(a);NativeBridge.audio.open(44100);NativeBridge.playing=true;
        Thread writer=new Thread(()->{
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);
            int frames=2205;float[] pcm=new float[frames*2];long n=0,next=android.os.SystemClock.elapsedRealtime()+10000;
            while(!a.isFinishing()){
                for(int i=0;i<frames;i++,n++)pcm[i*2]=pcm[i*2+1]=(float)(.02*Math.sin(2*Math.PI*440*n/44100));
                NativeBridge.audio.write(pcm);
                if(android.os.SystemClock.elapsedRealtime()>=next){next+=10000;android.util.Log.i("TonalTest","AUDIO_LOAD frames="+n+" underruns="+NativeBridge.audio.underruns());}
            }
        },"TonalFixtureTone");writer.start();
    }
    private static void stats(MainActivity a,SpotifyClient api,MusicStore store){
        JSONObject track=new JSONObject();MusicStore.put(track,"provider","Spotify");MusicStore.put(track,"id","synthetic-story-0150");MusicStore.put(track,"title","Canção de demonstração");MusicStore.put(track,"artist","Artista sintético");MusicStore.put(track,"album","Álbum de teste · dados sintéticos");MusicStore.put(track,"genre","grunge, alternative rock");MusicStore.put(track,"durationMs",180000);store.select(track);
        JSONArray events=new JSONArray();for(int i=0;i<20;i++){JSONObject e=new JSONObject();MusicStore.put(e,"session","story-ui-"+i);MusicStore.put(e,"key",MusicStore.key(track));MusicStore.put(e,"at",System.currentTimeMillis()-i*3600000L);MusicStore.put(e,"ms",120000);MusicStore.put(e,"play",true);events.put(e);}MusicStore.put(store.data,"events",events);store.save();
        store.stories().capture(track,"Uma canção sobre voltar à casa da infância, recordar uma amizade distante e seguir em frente. A saudade se mistura à esperança de um novo caminho e à coragem de recomeçar.");MusicPanel.showStats(a,api);
    }
    static void setlists(MainActivity a,SpotifyClient api){SetlistWizard.show(a,api);}
}
