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
    private final Messenger endpoint=new Messenger(new Handler(Looper.getMainLooper(),message->{
        if(message.what!=GENERATE||working||message.replyTo==null)return true;
        Bundle b=message.getData();String[] texts=b.getStringArray("texts");Messenger reply=message.replyTo;
        if(texts==null||texts.length<1||texts.length>8)return true;
        for(String text:texts)if(text==null||text.getBytes(StandardCharsets.UTF_8).length>4096)return true;
        working=true;worker.execute(()->{
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
            String output;
            try {
                File model=new File(getFilesDir(),"stories-model/qwen2.5-0.5b-instruct-q4_k_m.gguf");
                if(model.length()!=LocalStories.MODEL_BYTES)throw new IllegalStateException();
                System.loadLibrary("tonal_story");byte[][] input=new byte[texts.length][];
                for(int i=0;i<texts.length;i++)input[i]=texts[i].getBytes(StandardCharsets.UTF_8);
                output=new String(generate(model.getAbsolutePath().getBytes(StandardCharsets.UTF_8),input,45000),StandardCharsets.UTF_8);
            }catch(Throwable error){output="ERROR:Não foi possível carregar o modelo local. Confira o download e a memória disponível.";}
            try {Message result=Message.obtain(null,RESULT);Bundle data=new Bundle();data.putString("output",output.substring(0,Math.min(1800,output.length())));result.setData(data);reply.send(result);}catch(RemoteException ignored){}
        });return true;
    }));
    static native byte[] generate(byte[] model,byte[][] texts,int budgetMs);
    @Override public IBinder onBind(Intent intent){return endpoint.getBinder();}
    @Override public boolean onUnbind(Intent intent){release();return false;}
    @Override public void onDestroy(){worker.shutdownNow();super.onDestroy();release();}
    private void release(){if(Application.getProcessName().endsWith(":stories"))android.os.Process.killProcess(android.os.Process.myPid());}
}
