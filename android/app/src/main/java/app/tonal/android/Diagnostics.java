package app.tonal.android;

import android.content.Context;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

/** Bounded private diagnostics; no OAuth, full URLs or library credentials. */
final class Diagnostics {
    private static File directory;
    private static final ThreadPoolExecutor writer=new ThreadPoolExecutor(1,1,20,TimeUnit.SECONDS,new ArrayBlockingQueue<>(128),r->{Thread t=new Thread(r,"tonal-diagnostics");t.setDaemon(true);return t;},new ThreadPoolExecutor.DiscardPolicy());
    static void initialize(Context context,String module){directory=new File(context.getFilesDir(),"diagnostics/"+module);}
    static String sanitize(String value){return value.replaceAll("(?i)https?://[^\\s]+","<url>").replaceAll("(?i)(bearer\\s+|(?:access_token|refresh_token|password|token|authorization)[\\s\"':=]+)[^\\s,;}]+","$1<redacted>").replaceAll("[A-Za-z0-9_+/=-]{32,}","<redacted>");}
    static void write(String value){File dir=directory;if(dir==null)return;String safe=sanitize(value);writer.execute(()->{try{dir.mkdirs();File log=new File(dir,"playback.log");if(log.length()>256*1024){File old=new File(dir,"playback.log.1");if(old.exists()&&!old.delete())return;if(!log.renameTo(old))return;}try(FileOutputStream out=new FileOutputStream(log,true)){out.write((System.currentTimeMillis()+" "+safe.substring(0,Math.min(2048,safe.length()))+"\n").getBytes(StandardCharsets.UTF_8));}}catch(IOException ignored){}});}
    private Diagnostics(){}
}
