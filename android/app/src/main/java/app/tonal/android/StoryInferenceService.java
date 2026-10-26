package app.tonal.android;

import android.app.*;
import android.content.*;
import android.os.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

/** Bound only while the lyric reader is open. No audio, tokens or network here. */
public final class StoryInferenceService extends Service {
    static final int GENERATE=1,RESULT=2;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private boolean working;
    private final Handler monitor=new Handler(Looper.getMainLooper());
    private final Runnable pressure=new Runnable(){public void run(){if(!working)return;ActivityManager.MemoryInfo info=new ActivityManager.MemoryInfo();((ActivityManager)getSystemService(ACTIVITY_SERVICE)).getMemoryInfo(info);if(info.lowMemory||info.availMem<256L*1024*1024){release();return;}monitor.postDelayed(this,1000);}};
    private final Messenger endpoint=new Messenger(new Handler(Looper.getMainLooper(),message->{
        if(message.what!=GENERATE||message.replyTo==null)return true;
        if(working){send(message.replyTo,"ERROR:Outro leitor está usando o modelo. Aguarde essa análise terminar.");return true;}
        Bundle b=message.getData();String[] texts=b.getStringArray("texts"),known=b.getStringArray("known");Messenger reply=message.replyTo;
        String checkpoint=b.getString("checkpoint",""),key=b.getString("batchKey","");
        final int budget=BuildConfig.DEBUG?Math.max(1000,Math.min(90000,b.getInt("budgetMs",90000))):90000;
        try{File f=new File(checkpoint).getCanonicalFile();if(!key.matches("[a-f0-9]{64}")||!f.getName().equals("resume-v5.bin")||!f.toPath().startsWith(getCacheDir().getCanonicalFile().toPath())){send(reply,"ERROR:Lote de retomada inválido.");return true;}}catch(java.io.IOException bad){send(reply,"ERROR:Não foi possível preparar a retomada.");return true;}
        if(texts==null||texts.length<1||texts.length>8)return true;
        for(String text:texts)if(text==null||text.getBytes(StandardCharsets.UTF_8).length>4096)return true;
        if(known!=null){if(known.length!=texts.length)return true;for(String text:known)if(text==null||text.getBytes(StandardCharsets.UTF_8).length>700)return true;}
        working=true;monitor.postDelayed(pressure,1000);worker.execute(()->{
            // Keep background CPU niceness without forcing Android's restrictive I/O
            // task profile onto mmap loading. The service remains NOT_FOREGROUND.
            // The pause-only JNI entry applies nice +2 through Bionic setpriority before any
            // model loading; failure returns an error instead of running at full priority.
            String output;
            try {
                File model=new File(getFilesDir(),"stories-model/Qwen3.5-2B-Q4_K_M.gguf");
                ActivityManager.MemoryInfo memory=new ActivityManager.MemoryInfo();((ActivityManager)getSystemService(ACTIVITY_SERVICE)).getMemoryInfo(memory);
                if(memory.lowMemory||memory.availMem<LocalStories.MODEL_BYTES+512L*1024*1024)throw new IllegalStateException();
                if(model.length()!=LocalStories.MODEL_BYTES)throw new IllegalStateException();
                if(BuildConfig.DEBUG){
                    android.system.Os.setenv("TONAL_STORY_TRACE","1",true);
                    try(java.io.FileOutputStream trace=new java.io.FileOutputStream(new File(getCacheDir(),"story-native-trace.log"))){android.system.Os.dup2(trace.getFD(),2);}
                    android.util.Log.i("TonalStory","native-load");
                }
                System.loadLibrary("tonal_story");if(BuildConfig.DEBUG)android.util.Log.i("TonalStory","native-generate");byte[][] input=new byte[texts.length][];
                byte[][] prior=known==null?null:new byte[known.length][];
                for(int i=0;i<texts.length;i++){input[i]=texts[i].getBytes(StandardCharsets.UTF_8);if(prior!=null)prior[i]=known[i].getBytes(StandardCharsets.UTF_8);}
                byte[] location=model.getAbsolutePath().getBytes(StandardCharsets.UTF_8),state=checkpoint.getBytes(StandardCharsets.UTF_8),identity=key.getBytes(StandardCharsets.UTF_8);
                boolean accelerated=supportsAcceleratedCpu();
                if(accelerated){try{System.loadLibrary("tonal_story_fast");}catch(UnsatisfiedLinkError missing){accelerated=false;}}
                if(BuildConfig.DEBUG)android.util.Log.i("TonalStory",accelerated?"backend=ARM-DotProd-I8MM":"backend=baseline");
                output=new String(accelerated?generateFast(location,input,prior,budget,state,identity):generate(location,input,prior,budget,state,identity),StandardCharsets.UTF_8);
            }catch(Throwable error){output="ERROR:Não foi possível carregar o modelo local. Confira o download e a memória disponível.";}
            if(BuildConfig.DEBUG)android.util.Log.i("TonalStory","native-finished");
            try {Message result=Message.obtain(null,RESULT);Bundle data=new Bundle();data.putString("output",output.substring(0,Math.min(16384,output.length())));result.setData(data);reply.send(result);}catch(RemoteException ignored){}
        });return true;
    }));
    static native boolean supportsAcceleratedCpu();
    private static void send(Messenger reply,String output){try{Message result=Message.obtain(null,RESULT);Bundle data=new Bundle();data.putString("output",output);result.setData(data);reply.send(result);}catch(RemoteException ignored){}}
    static native byte[] generate(byte[] model,byte[][] texts,byte[][] known,int budgetMs,byte[] checkpoint,byte[] key);
    static native byte[] generateFast(byte[] model,byte[][] texts,byte[][] known,int budgetMs,byte[] checkpoint,byte[] key);
    @Override public IBinder onBind(Intent intent){return endpoint.getBinder();}
    @Override public boolean onUnbind(Intent intent){release();return false;}
    @Override public void onDestroy(){worker.shutdownNow();super.onDestroy();release();}
    private void release(){monitor.removeCallbacks(pressure);if(Application.getProcessName().endsWith(":stories"))android.os.Process.killProcess(android.os.Process.myPid());}
}
