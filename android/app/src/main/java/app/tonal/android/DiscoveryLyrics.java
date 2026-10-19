package app.tonal.android;

import android.util.AtomicFile;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;

/** Derived topic cache, independent of displayed lyrics, playback and personal history. */
final class DiscoveryLyrics {
    private final AtomicFile file;
    private final ExecutorService disk;
    private final ThreadPoolExecutor worker;
    private final JSONObject cache=new JSONObject();
    private volatile boolean enabled=true;
    private volatile int generation,consulted,analyzed,pending;
    private volatile HttpURLConnection active;
    private volatile long nextRequestAt;
    DiscoveryLyrics(File path,ExecutorService disk){
        this.file=new AtomicFile(path);this.disk=disk;
        worker=new ThreadPoolExecutor(1,1,5,TimeUnit.SECONDS,new ArrayBlockingQueue<>(1),r->{
            Thread t=new Thread(r,"tonal-discovery-lyrics");t.setPriority(Thread.MIN_PRIORITY);return t;
        },new ThreadPoolExecutor.DiscardOldestPolicy());worker.allowCoreThreadTimeOut(true);
        try{if(path.length()<=2*1024*1024){JSONObject root=new JSONObject(new String(file.readFully(),StandardCharsets.UTF_8));
            if(root.optInt("version")==1){enabled=root.optBoolean("enabled",true);JSONObject entries=MusicStore.object(root,"entries");
                for(Iterator<String> i=entries.keys();i.hasNext()&&cache.length()<2000;){String k=i.next();if(k.length()==64&&entries.opt(k) instanceof JSONObject)MusicStore.put(cache,k,entries.opt(k));}
            }
        }}catch(Exception ignored){}
    }
    static String signature(JSONObject t){try{
        JSONArray parts=new JSONArray().put(t.optString("uri")).put(t.optString("title")).put(t.optString("artist")).put(t.optString("album")).put(t.optInt("durationMs"));
        byte[] bytes=MessageDigest.getInstance("SHA-256").digest(parts.toString().getBytes(StandardCharsets.UTF_8));StringBuilder out=new StringBuilder();for(byte b:bytes)out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();
    }catch(Exception impossible){throw new AssertionError(impossible);}}
    static boolean matches(JSONObject t,JSONObject o){
        double duration=o.optDouble("duration",Double.NaN);
        return IndieCatalog.normalize(t.optString("title")).equals(IndieCatalog.normalize(o.optString("trackName")))&&
            IndieCatalog.normalize(t.optString("artist")).equals(IndieCatalog.normalize(o.optString("artistName")))&&
            Double.isFinite(duration)&&Math.abs(t.optInt("durationMs")/1000.-duration)<=5;
    }
    synchronized JSONObject lookup(JSONObject t){
        JSONObject e=MusicStore.object(cache,signature(t));if(e.optLong("until")<=System.currentTimeMillis()||!e.optBoolean("analyzed"))return new JSONObject();
        JSONObject result=new JSONObject(),topics=new JSONObject(),raw=MusicStore.object(e,"topics");
        for(String name:MusicInsightModel.LABELS){double value=raw.optDouble(name,-1);if(Double.isFinite(value)&&value>=0&&value<=1)MusicStore.put(topics,name,value);}
        MusicStore.put(result,"lyricsAnalyzed",true);MusicStore.put(result,"topics",topics);return result;
    }
    boolean enabled(){return enabled;}
    void setEnabled(boolean value){if(enabled==value)return;enabled=value;generation++;worker.getQueue().clear();pending=0;HttpURLConnection c=active;if(c!=null)c.disconnect();persist();}
    String status(){return enabled?"Letras da descoberta • "+consulted+" consultadas • "+analyzed+" analisadas • "+pending+" pendentes":"Análise automática de letras desativada";}
    void enqueue(List<JSONObject> references,List<JSONObject> candidates){
        if(!enabled)return;ArrayList<JSONObject> tasks=new ArrayList<>(),all=new ArrayList<>();
        all.addAll(references.subList(0,Math.min(12,references.size())));all.addAll(candidates.subList(0,Math.min(120,candidates.size())));
        HashSet<String> seen=new HashSet<>();synchronized(this){for(JSONObject t:all){String k=signature(t);
            if(tasks.size()>=144)break;if(!t.optString("uri").matches("spotify:track:[A-Za-z0-9]{22}")||t.optString("title").trim().isEmpty()||t.optString("artist").trim().isEmpty()||t.optInt("durationMs")<=0||t.optInt("durationMs")>3600000||!seen.add(k))continue;
            if(MusicStore.object(cache,k).optLong("until")>System.currentTimeMillis())continue;
            try{tasks.add(new JSONObject(t.toString()));}catch(JSONException ignored){}
        }}
        int serial=++generation;pending=tasks.size();worker.getQueue().clear();HttpURLConnection previous=active;if(previous!=null)previous.disconnect();
        worker.execute(()->{
            for(JSONObject t:tasks){if(!awaitRequest(serial))break;
                int code=0,retry=0;JSONObject response=new JSONObject();
                try{
                    String address=WebRequest.params("https://lrclib.net/api/get","track_name",t.optString("title"),"artist_name",t.optString("artist"),"album_name",t.optString("album"),"duration",String.valueOf(Math.round(t.optInt("durationMs")/1000.)));
                    HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection();active=c;c.setInstanceFollowRedirects(false);c.setConnectTimeout(4000);c.setReadTimeout(4000);
                    c.setRequestProperty("User-Agent","TonalHub/0.12.0 (https://github.com/mikerock12/tonal-hub)");
                    ScheduledFuture<?> deadline=WebRequest.deadlines.schedule(c::disconnect,6,TimeUnit.SECONDS);
                    try{code=c.getResponseCode();try{retry=Math.max(0,Math.min(60,Integer.parseInt(c.getHeaderField("Retry-After"))));}catch(Exception ignored){}
                        if(code==200)try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){
                            byte[] buffer=new byte[4096];for(int n;(n=in.read(buffer))!=-1;){if(out.size()+n>256*1024)throw new IOException("Resposta excessiva");out.write(buffer,0,n);}
                            response=new JSONObject(new String(out.toByteArray(),StandardCharsets.UTF_8));
                        }
                    }finally{deadline.cancel(false);c.disconnect();active=null;}
                }catch(Exception ignored){}
                // A new selection must not bypass an existing provider cooldown.
                long delay=code==429||code==503?Math.max(10000,Math.min(60000,retry*1000L)):1250;
                nextRequestAt=Math.max(nextRequestAt,System.nanoTime()+delay*1000000);
                if(serial!=generation||!enabled)break;
                boolean found=code==200&&matches(t,response);String text=found?response.optString("plainLyrics",""):"";if(text.equals("null"))text="";
                if(found&&text.isEmpty())text=response.optString("syncedLyrics","").replaceAll("\\[[^\\]]*\\]","");if(text.equals("null"))text="";
                JSONObject entry=new JSONObject();MusicStore.put(entry,"analyzed",found&&!text.trim().isEmpty());
                MusicStore.put(entry,"until",System.currentTimeMillis()+(found?30L*86400000:code==404?86400000:900000));
                if(found&&!text.trim().isEmpty()){MusicStore.put(entry,"topics",MusicInsightModel.analyze(text));analyzed++;}
                synchronized(this){MusicStore.put(cache,signature(t),entry);while(cache.length()>2000){String oldest=null;long expires=Long.MAX_VALUE;for(Iterator<String> i=cache.keys();i.hasNext();){String k=i.next();long until=MusicStore.object(cache,k).optLong("until");if(until<expires){expires=until;oldest=k;}}if(oldest==null)break;cache.remove(oldest);}}
                consulted++;pending=Math.max(0,pending-1);persist();
            }
            if(serial==generation)pending=0;
        });
    }
    private boolean awaitRequest(int serial){
        try{while(serial==generation&&enabled&&System.nanoTime()<nextRequestAt)
            Thread.sleep(Math.min(250,Math.max(1,(nextRequestAt-System.nanoTime())/1000000)));
        }catch(InterruptedException stop){Thread.currentThread().interrupt();return false;}
        return serial==generation&&enabled;
    }
    private void persist(){
        final byte[] bytes;synchronized(this){JSONObject root=new JSONObject();MusicStore.put(root,"version",1);MusicStore.put(root,"enabled",enabled);MusicStore.put(root,"entries",cache);bytes=root.toString().getBytes(StandardCharsets.UTF_8);}
        disk.execute(()->{FileOutputStream out=null;try{out=file.startWrite();out.write(bytes);file.finishWrite(out);}catch(Exception error){if(out!=null)file.failWrite(out);}});
    }
}
