package app.tonal.android;

import android.content.Context;
import java.io.*;
import java.nio.file.*;
import java.util.concurrent.atomic.AtomicLong;
import org.json.JSONObject;

/** File decoding/analysis stays off the UI and streaming threads. One bounded, cancellable job. */
final class AudioFiles {
    static final AtomicLong serial=new AtomicLong();
    static volatile String current="";
    static long cancel(){long n=serial.incrementAndGet();NativeAudio.cancel(n);return n;}
    static boolean iamf(File file)throws IOException {try(InputStream input=new FileInputStream(file)){byte[] b=new byte[32];int n=input.read(b);if(n<8||((b[0]&255)>>>3)!=31)return false;for(int i=2;i+3<n&&i<12;i++)if(b[i]=='i'&&b[i+1]=='a'&&b[i+2]=='m'&&b[i+3]=='f')return true;return false;}}
    static final class Prepared {
        final File file;final JSONObject info;
        Prepared(File f,JSONObject i){file=f;info=i;}
        void activate(){current=file.getAbsolutePath();NativeBridge.audio.source(true,info.optBoolean("binaural"),info.optDouble("lufs",-70),info.optDouble("peak"),info.optDouble("replayGain",Double.NaN),info.optDouble("replayPeak"));}
    }
    static Prepared prepare(Context context,File input,long generation)throws Exception {
        File dir=new File(context.getCacheDir(),"prepared-audio");if(!dir.isDirectory()&&!dir.mkdirs())throw new IOException("Cache de áudio indisponível");
        File[] old=dir.listFiles();if(old!=null)for(File f:old)if(!f.getAbsolutePath().equals(current))f.delete();
        File temp=new File(dir,"audio-"+generation+".part"),output=new File(dir,"audio-"+generation+".wav");
        try{
            JSONObject info=new JSONObject(NativeAudio.prepare(input.getAbsolutePath(),temp.getAbsolutePath(),iamf(input),generation));
            if(serial.get()!=generation)throw new IOException("Preparação cancelada");
            Files.move(temp.toPath(),output.toPath(),StandardCopyOption.REPLACE_EXISTING);return new Prepared(output,info);
        }finally{temp.delete();if(input.getName().startsWith("audio-open-"))input.delete();}
    }
}
