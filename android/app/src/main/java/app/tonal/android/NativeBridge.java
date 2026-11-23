package app.tonal.android;

import android.os.Handler;
import android.os.Looper;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;

public final class NativeBridge {
    static { System.loadLibrary("tonal_android"); }
    private static native void nativeStart(String token, String cache, int bitrate, boolean normalize);
    static native void sendCommand(String command);
    static volatile Consumer<String> commandListener;
    static volatile Consumer<String> jamListener;
    static void jamRequest(String json){if(ready&&json.length()<=8192)sendCommand("jam|"+json);}
    public static void onJamResult(long epoch,String json){if(epoch!=generation||json.length()>600*1024)return;main.post(()->{Consumer<String> listener=jamListener;if(epoch==generation&&listener!=null)listener.accept(json);});}
    /** Engine position events (Seeked/PositionChanged), delivered on the main thread. */
    static volatile java.util.function.BiConsumer<String,Integer> progressObserver;
    static volatile java.util.function.BiConsumer<Integer,Boolean> queueEditObserver;
    static final CopyOnWriteArrayList<Consumer<String>> commandListeners=new CopyOnWriteArrayList<>();
    static final PlaybackIntent intent=new PlaybackIntent();
    static volatile long generation,lastPcm;
    static volatile boolean spotifyEngine;
    public static void start(String token,String cache,int bitrate,boolean normalize){spotifyEngine=true;nativeStart(token,cache,bitrate,normalize);}
    public static void command(String command){
        synchronized(intent){intent.command(command,playing);}
        if(PlaybackIntent.movesAudio(command)){Transitions.cancel();audio.discontinuity();audio.forgetTrackPosition();}
        if(command.equals("pause")||command.equals("stop")){audio.pause();playing=false;}
        repeatMode=RepeatMode.afterCommand(repeatMode,command);
        Consumer<String> listener=commandListener;if(listener!=null)listener.accept(command);
        main.post(()->{for(Consumer<String> observer:commandListeners)observer.accept(command);});
        sendCommand(command);
        // A new selection (including a recovery restore) resets repeat in librespot.
        if(RepeatMode.loadsSelection(command)&&repeatMode!=RepeatMode.OFF)applyRepeat();
    }
    /** RepeatMode.OFF/QUEUE/TRACK; MainActivity loads and saves the choice. */
    static volatile int repeatMode;
    static void applyRepeat(){for(String c:RepeatMode.engineCommands(repeatMode))sendCommand(c);}
    public static void onGeneration(long value){generation=value;}
    public static void onQueueEditResult(long epoch,int revision,boolean accepted){
        main.post(()->{
            var observer=queueEditObserver;
            if(epoch==generation&&observer!=null)observer.accept(revision,accepted);
        });
    }
    public static void onNativeStatus(long epoch,String value){if(epoch==generation)onStatus(value);}
    public static void onProgress(long epoch,String uri,int position,boolean active){
        if(epoch!=generation)return;
        long eventAt=android.os.SystemClock.elapsedRealtime();
        // Pausa vinda de outro controle: com a transição o som está adiantado no buffer, pausa já.
        if(!active&&audio.transitionState()!=0){audio.pause();playing=false;}
        main.post(()->{if(epoch!=generation)return;synchronized(intent){intent.progress(uri,position,active);}java.util.function.BiConsumer<String,Integer> observer=progressObserver;if(observer!=null)observer.accept(uri,position);Transitions.onProgress(uri,position,active,eventAt);});
    }
    /** Faixa carregada pelo motor, com a duração do catálogo (TrackChanged). */
    public static void onTrackInfo(long epoch,String uri,int duration){
        if(epoch!=generation)return;
        main.post(()->{if(epoch==generation)Transitions.onTrack(uri,duration);});
    }
    public static native void playFile(String path);
    static final AudioOutput audio = new AudioOutput();
    static final Handler main = new Handler(Looper.getMainLooper());
    static final CopyOnWriteArrayList<Consumer<String>> listeners = new CopyOnWriteArrayList<>();
    static volatile String status = "Pronto para conectar ou abrir um arquivo";
    static volatile boolean ready, playing;
    private static boolean engineTempCleaned;
    static synchronized void prepareEngineCache(java.io.File cache){
        if(engineTempCleaned)return;
        // Only orphaned range files from an earlier process exist before the
        // first native engine starts. Keep completed audio and credentials.
        java.io.File[] leftovers=new java.io.File(cache,"tmp").listFiles();
        if(leftovers!=null)for(java.io.File file:leftovers)
            if(java.nio.file.Files.isRegularFile(file.toPath(),java.nio.file.LinkOption.NOFOLLOW_LINKS))file.delete();
        engineTempCleaned=true;
    }
    public static void onStatus(String value) {
        status=value;
        if(value.startsWith("Pronto •")) ready=true;
        if(value.startsWith("Falha") || value.startsWith("Tempo") || value.startsWith("Sessão")) {ready=false;playing=false;audio.pause();}
        long epoch=generation;
        Diagnostics.write(value);
        main.post(()->{ if(epoch==generation)for(Consumer<String> listener:listeners) listener.accept(value); });
    }
    public static void onAudioStart(int rate) {
        boolean allowed;synchronized(intent){allowed=intent.audioStarted(android.os.SystemClock.elapsedRealtime());}
        if(!allowed){playing=false;audio.pause();sendCommand("pause");onDiagnostic("Áudio não solicitado bloqueado");return;}
        audio.source(false,false,-70,0,Double.NaN,0); audio.startStereo(rate); playing=true; onStatus("Reproduzindo");
    }
    public static void onAudioFormat(int rate,int channels) { audio.open(rate,channels); playing=true; onStatus("Reproduzindo"); }
    public static void onAudioResume() { if(spotifyEngine&&!intent.desired){sendCommand("pause");return;}audio.resume(); playing=true; onStatus("Reproduzindo"); }
    public static void onAudioPause() {
        // Fim natural com transição: o motor fecha o sink, mas o fim da faixa
        // já lido continua tocando até a próxima chegar ou a mescla terminar.
        boolean continuing;synchronized(intent){continuing=spotifyEngine&&intent.desired;}
        if(continuing&&audio.continuesThroughEngineStop())return;
        audio.pause(); playing=false; onStatus("Pausado");
    }
    /** A mescla terminou sem a próxima faixa (fim da fila ou sink ainda parado). */
    static void onTransitionDrained(){
        boolean stopped;synchronized(intent){stopped=!intent.desired;}
        if(stopped||android.os.SystemClock.elapsedRealtime()-lastPcm>250){audio.pause();playing=false;onStatus("Pausado");}
    }
    /** Começo exato de uma faixa no PCM (marcador do librespot), antes da primeira amostra dela. */
    public static void onTrackBoundary(int positionMs){if(spotifyEngine)audio.boundary(positionMs);}
    /** Transporte efetivamente aplicado pelo player, na ordem do PCM e da geração da sessão. */
    public static void onTransportEvent(long epoch,int event){
        if(epoch!=generation||!spotifyEngine)return;
        if(event==1||event==2||event==3){
            Transitions.cancel();audio.transportDiscontinuity();audio.forgetTrackPosition();
        }
    }
    /** Engine closed the sink only to load another track; runs right after onAudioPause. */
    public static void onAudioDiscontinuity() { audio.discontinuity(); }
    private static final ThreadLocal<Boolean> audioPriority=ThreadLocal.withInitial(()->false);
    public static void onPcm(float[] samples) {
        if(spotifyEngine&&!intent.desired)return;
        if(samples.length>0)lastPcm=android.os.SystemClock.elapsedRealtime();
        if(!audioPriority.get()){try{android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);}catch(SecurityException ignored){}audioPriority.set(true);}
        audio.write(samples);
    }
    public static void onAudioEnd(){audio.finish();playing=false;}
    public static void onDiagnostic(String value) { Diagnostics.write(value);if(BuildConfig.DEBUG)android.util.Log.w("TonalEngine",Diagnostics.sanitize(value)); }
    private NativeBridge() {}
}
