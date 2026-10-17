package app.tonal.android;

import android.media.MediaDataSource;
import android.os.SystemClock;
import java.io.*;
import java.net.HttpURLConnection;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** Private, bounded disk reserve for platform decoders. PCM is never stored in RAM as a song. */
final class ProgressiveSource extends MediaDataSource {
    static final int CHUNK=128*1024,MAX_CHUNKS=256,AHEAD=64;
    private final File directory;private final String url;
    private final Consumer<String> status;
    private final LinkedHashMap<Long,File> chunks=new LinkedHashMap<>(16,.75f,true);
    private volatile boolean closed,paused;private volatile HttpURLConnection connection;
    private InputStream sequential;private long sequentialIndex;
    private volatile long length=-1;
    private boolean ranges=true,ended;private long demand,playhead;
    private final Thread fetcher;private String failure="";private boolean fatal;
    ProgressiveSource(File cache,String address,Consumer<String> observer)throws IOException{
        status=observer;url=WebRequest.url(address).toString();directory=new File(cache,"stream-"+UUID.randomUUID());if(!directory.mkdirs())throw new IOException("Não foi possível criar o buffer");
        try{fetch(0);}catch(IOException e){close();throw e;}
        fetcher=new Thread(this::download,"indie-reserve");fetcher.setDaemon(true);fetcher.start();
    }
    private void download(){try{int attempts=0;while(!closed){long index;
        synchronized(this){if(paused){try{wait(1000);}catch(InterruptedException e){return;}continue;}index=-1;if(!ranges){if(!chunks.containsKey(demand)&&demand<sequentialIndex){closeSequential();ranges=true;ended=false;index=demand;}else if(!ended&&sequentialIndex<playhead+AHEAD)index=sequentialIndex;}else if(!chunks.containsKey(demand)&&!atEnd(demand))index=demand;else for(long n=playhead;n<playhead+AHEAD&&!atEnd(n);n++)if(!chunks.containsKey(n)){index=n;break;}
            if(index<0){try{wait(500);}catch(InterruptedException e){return;}continue;}}
        try{fetch(index);attempts=0;synchronized(this){failure="";notifyAll();}}catch(IOException e){if(closed)return;attempts++;closeSequential();ranges=true;boolean permanent=e instanceof WebRequest.Failure&&Arrays.asList(400,401,403,404,410,416).contains(((WebRequest.Failure)e).code);synchronized(this){fatal=permanent;failure=permanent?"O servidor não disponibiliza este áudio":"Aguardando conexão para preencher o buffer";notifyAll();}if(permanent)return;Diagnostics.write("Indie buffer: "+(e instanceof WebRequest.Failure?e.getMessage():e.getClass().getSimpleName())+"; tentativa "+attempts);try{Thread.sleep(Math.min(8000,500L<<Math.min(4,attempts)));}catch(InterruptedException interrupted){return;}}
    }}finally{if(closed)cleanup();}}
    private void closeSequential(){InputStream in=sequential;sequential=null;HttpURLConnection c=connection;if(c!=null)c.disconnect();if(in!=null)try{in.close();}catch(IOException ignored){}}
    private boolean atEnd(long index){return length>=0&&index*CHUNK>=length;}
    private void fetch(long index)throws IOException{
        InputStream input;HttpURLConnection c=null;boolean keep=false;
        if(!ranges&&sequential!=null){if(index!=sequentialIndex)throw new IOException("Fluxo sequencial fora de ordem");input=sequential;c=connection;keep=true;}
        else{
            c=WebRequest.open(url,"bytes="+(index*CHUNK)+"-"+((index+1)*CHUNK-1));connection=c;
            if(c.getResponseCode()==206){String range=c.getHeaderField("Content-Range");if(range==null||!range.startsWith("bytes "+index*CHUNK+"-")){c.disconnect();throw new IOException("Intervalo de áudio inválido");}try{length=Long.parseLong(range.substring(range.lastIndexOf('/')+1));}catch(NumberFormatException e){c.disconnect();throw new IOException("Tamanho do áudio inválido");}input=c.getInputStream();}
            else{ranges=false;length=c.getContentLengthLong();sequential=c.getInputStream();sequentialIndex=index;input=sequential;keep=true;
                if(index>0){final HttpURLConnection initial=c;ScheduledFuture<?> skipTimeout=WebRequest.deadlines.schedule(initial::disconnect,25,TimeUnit.SECONDS);try{byte[] skip=new byte[16384];long remaining=index*CHUNK;while(remaining>0){if(closed)throw new InterruptedIOException();int n=input.read(skip,0,(int)Math.min(skip.length,remaining));if(n<0)throw new EOFException();remaining-=n;}}finally{skipTimeout.cancel(false);}}}
        }
        final HttpURLConnection active=c;ScheduledFuture<?> timeout=WebRequest.deadlines.schedule(active::disconnect,25,TimeUnit.SECONDS);
        byte[] data=new byte[CHUNK];int count=0;File temporary=new File(directory,index+".part");
        try{while(count<data.length){if(closed)throw new InterruptedIOException();int n=input.read(data,count,data.length-count);if(n<0)break;count+=n;}
            if(ranges&&length>=0&&count!=Math.min(CHUNK,length-index*CHUNK))throw new IOException("Intervalo de áudio incompleto");
            if(count==0){synchronized(this){ended=true;if(length<0)length=index*CHUNK;notifyAll();}return;}
            try(FileOutputStream out=new FileOutputStream(temporary)){out.write(data,0,count);}File file=new File(directory,index+".chunk");if(!temporary.renameTo(file))throw new IOException("Não foi possível salvar a reserva");
            synchronized(this){chunks.put(index,file);while(chunks.size()>MAX_CHUNKS){Long oldest=chunks.keySet().iterator().next();File old=chunks.remove(oldest);old.delete();}if(!ranges){sequentialIndex=index+1;if(count<CHUNK){ended=true;length=index*CHUNK+count;}}notifyAll();}
        }finally{timeout.cancel(false);temporary.delete();if(!keep){try{input.close();}finally{active.disconnect();connection=null;}}}
    }
    @Override public int readAt(long position,byte[] bytes,int offset,int size)throws IOException{
        if(position<0||offset<0||size<0||offset+size>bytes.length)throw new IOException("Leitura inválida");if(size==0)return 0;
        long index=position/CHUNK;boolean waiting=false;
        synchronized(this){demand=index;playhead=index;notifyAll();while(!closed&&!chunks.containsKey(index)&&!atEnd(index)){
            if(fatal)throw new IOException(failure);if(!failure.isEmpty()&&!waiting){waiting=true;status.accept("Aguardando conexão • a faixa será retomada");}try{wait(1000);}catch(InterruptedException e){Thread.currentThread().interrupt();throw new InterruptedIOException();}}
            if(closed)throw new InterruptedIOException();if(atEnd(index))return -1;File file=chunks.get(index);if(file==null)return -1;
            if(waiting)status.accept("Reproduzindo • reserva progressiva");
            try(RandomAccessFile in=new RandomAccessFile(file,"r")){int within=(int)(position%CHUNK);if(within>=in.length())return -1;in.seek(within);return in.read(bytes,offset,(int)Math.min(size,in.length()-within));}
        }
    }
    @Override public long getSize(){return length;}
    synchronized long bufferedBytes(){long n=0;for(File f:chunks.values())n+=f.length();return n;}
    synchronized void pausePrefetch(boolean value){paused=value;notifyAll();}
    @Override public synchronized void close(){if(closed)return;closed=true;notifyAll();Thread fetching=fetcherOrNull();if(fetching!=null)fetching.interrupt();HttpURLConnection c=connection;Thread cancel=new Thread(()->{if(c!=null)c.disconnect();if(fetching!=null)try{fetching.join(2000);}catch(InterruptedException e){Thread.currentThread().interrupt();}if(fetching==null||!fetching.isAlive())cleanup();},"indie-cancel");cancel.setDaemon(true);cancel.start();}
    private void cleanup(){File[] files=directory.listFiles();if(files!=null)for(File f:files)f.delete();directory.delete();}
    private Thread fetcherOrNull(){return fetcher;}
}
