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
    static final CopyOnWriteArrayList<Consumer<String>> commandListeners=new CopyOnWriteArrayList<>();
    static final PlaybackIntent intent=new PlaybackIntent();
    static volatile long generation,lastPcm;
    public static void start(String token,String cache,int bitrate,boolean normalize){nativeStart(token,cache,bitrate,normalize);}
    public static void command(String command){
        synchronized(intent){intent.command(command,playing);}
        Consumer<String> listener=commandListener;if(listener!=null)listener.accept(command);
        main.post(()->{for(Consumer<String> observer:commandListeners)observer.accept(command);});
        sendCommand(command);
    }
    public static void onGeneration(long value){generation=value;}
    public static void onNativeStatus(long epoch,String value){if(epoch==generation)onStatus(value);}
    public static void onProgress(long epoch,String uri,int position,boolean active){
        if(epoch!=generation)return;
        main.post(()->{if(epoch!=generation)return;synchronized(intent){intent.progress(uri,position,active);}});
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
    public static void onAudioStart(int rate) { audio.source(false,false,-70,0,Double.NaN,0); audio.startStereo(rate); playing=true; onStatus("Reproduzindo"); }
    public static void onAudioFormat(int rate,int channels) { audio.open(rate,channels); playing=true; onStatus("Reproduzindo"); }
    public static void onAudioResume() { audio.resume(); playing=true; onStatus("Reproduzindo"); }
    public static void onAudioPause() { audio.pause(); playing=false; onStatus("Pausado"); }
    private static final ThreadLocal<Boolean> audioPriority=ThreadLocal.withInitial(()->false);
    public static void onPcm(float[] samples) {
        if(samples.length>0)lastPcm=android.os.SystemClock.elapsedRealtime();
        if(!audioPriority.get()){try{android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);}catch(SecurityException ignored){}audioPriority.set(true);}
        audio.write(samples);
    }
    public static void onAudioEnd(){audio.finish();playing=false;}
    public static void onDiagnostic(String value) { Diagnostics.write(value);if(BuildConfig.DEBUG)android.util.Log.w("TonalEngine",Diagnostics.sanitize(value)); }
    private NativeBridge() {}
}
