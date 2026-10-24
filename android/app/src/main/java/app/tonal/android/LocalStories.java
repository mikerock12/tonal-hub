package app.tonal.android;

import android.content.*;
import android.os.*;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.channels.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;

/** Bounded private evidence cache and optional native model. Never touches playback. */
final class LocalStories {
    static final long MODEL_BYTES=491400032L;
    static final String SHA="74a4da8c9fdbcd15bd1f6d01d621410d31c6fc00986f5eb687824e7b93d7a9db";
    private static final String URL_MODEL="https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/9217f5db79a29953eb74d5343926648285ec7e67/qwen2.5-0.5b-instruct-q4_k_m.gguf";
    private final Context app;private final File root,model,marker;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final ExecutorService worker=Executors.newSingleThreadExecutor(task->new Thread(()->{android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);task.run();},"Tonal-story-preparation"));
    private volatile boolean playing;private boolean resumeDownload;private volatile long cacheBytes,storedModelBytes;
    private final LinkedHashMap<String,java.util.AbstractMap.SimpleImmutableEntry<JSONObject,String>> pendingCaptures=new LinkedHashMap<>();private boolean captureQueued;
    private volatile int serial;private volatile HttpURLConnection connection;private long cooldown;
    private int period=7;private volatile boolean busy,downloading,ready;private boolean bound;private long downloaded;
    private String status="Resumo das letras no aparelho. O modelo é opcional.",summary="",sources="",cacheKey="";
    private String[] prepared;private Runnable observer;
    private ServiceConnection service;
    LocalStories(Context context,String module){
        app=context.getApplicationContext();root=new File(app.getCacheDir(),"stories-"+module);File dir=new File(app.getFilesDir(),"stories-model");model=new File(dir,"qwen2.5-0.5b-instruct-q4_k_m.gguf");marker=new File(dir,"verified.json");
        worker.execute(()->{new File(root,"lyrics").mkdirs();new File(root,"summaries").mkdirs();dir.mkdirs();boolean verified=model.length()==MODEL_BYTES&&SHA.equals(read(marker).optString("sha256"));refreshSizes();main.post(()->{ready=verified;notifyChanged();});});
    }
    void setPlaying(boolean value){if(playing==value)return;playing=value;if(value&&downloading){cancel();resumeDownload=true;downloading=true;status="Download aguardando uma pausa na reprodução.";notifyChanged();}else if(!value&&resumeDownload){resumeDownload=false;downloading=false;download();}}
    private void refreshSizes(){storedModelBytes=model.length();cacheBytes=bytes(new File(root,"lyrics"))+bytes(new File(root,"summaries"));}
    private boolean enoughMemory(){android.app.ActivityManager.MemoryInfo info=new android.app.ActivityManager.MemoryInfo();((android.app.ActivityManager)app.getSystemService(Context.ACTIVITY_SERVICE)).getMemoryInfo(info);return !info.lowMemory&&info.availMem>=MODEL_BYTES+512L*1024*1024;}
    private boolean awaitNetwork(int token)throws InterruptedException{while(playing){if(token!=serial)return false;main.post(()->{if(token==serial){status="Busca de letras aguardando uma pausa; o áudio tem prioridade.";notifyChanged();}});Thread.sleep(3000);}return token==serial;}
    void selectPeriod(int days){if(period!=days){cancel();period=days;summary=sources="";}}
    void resetSummary(){cancel();summary=sources="";}
    void observe(Runnable update){observer=update;}
    void detach(){observer=null;cancel();}
    private void notifyChanged(){if(observer!=null)observer.run();}
    JSONObject state(){JSONObject o=new JSONObject();MusicStore.put(o,"playbackPrioritized",playing);MusicStore.put(o,"ready",ready);MusicStore.put(o,"busy",busy);MusicStore.put(o,"downloading",downloading);MusicStore.put(o,"status",status);MusicStore.put(o,"summary",summary);MusicStore.put(o,"sources",sources);MusicStore.put(o,"progress",downloaded/(double)MODEL_BYTES);MusicStore.put(o,"modelBytes",storedModelBytes);MusicStore.put(o,"cacheBytes",cacheBytes);return o;}
    private static long bytes(File dir){long total=0;File[] files=dir.listFiles();if(files!=null)for(File f:files)total+=f.length();return total;}
    private static String hash(String value){try{return hex(MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8)));}catch(Exception error){throw new IllegalStateException(error);}}
    private static String hex(byte[] digest){StringBuilder b=new StringBuilder();for(byte v:digest)b.append(String.format(Locale.ROOT,"%02x",v&255));return b.toString();}
    private File lyricFile(JSONObject t){return new File(root,"lyrics/"+hash(MusicStore.key(t)+"|"+t.optString("title")+"|"+t.optString("artist")+"|"+t.optString("durationMs"))+".json");}
    private static JSONObject read(File f){try{if(f.length()>65536)return new JSONObject();return new JSONObject(new String(java.nio.file.Files.readAllBytes(f.toPath()),StandardCharsets.UTF_8));}catch(Exception ignored){return new JSONObject();}}
    private static void write(File file,JSONObject data)throws Exception{android.util.AtomicFile atomic=new android.util.AtomicFile(file);FileOutputStream stream=null;try{stream=atomic.startWrite();stream.write(data.toString().getBytes(StandardCharsets.UTF_8));atomic.finishWrite(stream);}catch(Exception error){atomic.failWrite(stream);throw error;}}
    private static void trim(File dir,int limit){File[] files=dir.listFiles(f->f.getName().endsWith(".json"));if(files==null)return;Arrays.sort(files,Comparator.comparingLong(File::lastModified).reversed());for(int i=limit;i<files.length;i++)files[i].delete();}
    void capture(JSONObject track,String lyrics){
        if(lyrics.length()<20)return;String key=MusicStore.key(track);String text=lyrics.substring(0,Math.min(6000,lyrics.length()));
        synchronized(pendingCaptures){pendingCaptures.put(key,new java.util.AbstractMap.SimpleImmutableEntry<>(track,text));while(pendingCaptures.size()>128)pendingCaptures.remove(pendingCaptures.keySet().iterator().next());if(captureQueued)return;captureQueued=true;}
        worker.execute(()->{for(;;){java.util.AbstractMap.SimpleImmutableEntry<JSONObject,String> entry;synchronized(pendingCaptures){if(pendingCaptures.isEmpty()){captureQueued=false;break;}String first=pendingCaptures.keySet().iterator().next();entry=pendingCaptures.remove(first);}try{write(lyricFile(entry.getKey()),new JSONObject().put("text",entry.getValue()));trim(new File(root,"lyrics"),128);}catch(Exception ignored){}}refreshSizes();});
    }
    private void unbind(){main.removeCallbacks(timeout);if(bound&&service!=null){try{app.unbindService(service);}catch(Exception ignored){}bound=false;}service=null;}
    void cancel(){resumeDownload=false;serial++;busy=downloading=false;HttpURLConnection active=connection;if(active!=null)active.disconnect();unbind();status="Operação cancelada. A reprodução continua normalmente.";notifyChanged();}
    void removeModel(){cancel();int token=serial;worker.execute(()->{boolean removed=model.delete()||!model.exists();if(removed)marker.delete();refreshSizes();main.post(()->{if(token!=serial)return;if(removed){ready=false;status="Modelo removido do aparelho.";}notifyChanged();});});}
    private final Runnable timeout=()->{if(busy){serial++;unbind();busy=false;status="A análise excedeu o limite. Tente novamente com menos músicas.";notifyChanged();}};
    private HttpURLConnection open(String address,int timeout)throws Exception{
        URL url=new URL(address);for(int i=0;i<6;i++){if(!"https".equals(url.getProtocol()))throw new IOException("HTTPS obrigatório");HttpURLConnection c=(HttpURLConnection)url.openConnection();connection=c;c.setConnectTimeout(timeout);c.setReadTimeout(timeout);c.setInstanceFollowRedirects(false);c.setRequestProperty("User-Agent","TonalHub/0.15.0 (local lyric analysis)");int code=c.getResponseCode();if(code>=300&&code<=399){String location=c.getHeaderField("Location");c.disconnect();url=new URL(url,location);continue;}return c;}throw new IOException("Redirecionamentos demais");
    }
    void download(){if(busy||downloading||ready)return;if(playing){resumeDownload=true;downloading=true;status="Download aguardando uma pausa na reprodução.";notifyChanged();return;}downloading=true;downloaded=0;int token=++serial;status="Baixando modelo opcional • 468,6 MiB";notifyChanged();
        worker.execute(()->{File partial=new File(model.getParentFile(),"download-"+android.os.Process.myPid()+".part");boolean ok=false;
            try(RandomAccessFile lockFile=new RandomAccessFile(new File(model.getParentFile(),"download.lock"),"rw");FileLock lock=lockFile.getChannel().tryLock()){
                if(model.getParentFile().getUsableSpace()<MODEL_BYTES+128L*1024*1024)throw new IOException("Espaço insuficiente");if(lock==null)throw new IOException("Outro player está baixando o modelo");HttpURLConnection c=open(URL_MODEL,30000);if(c.getResponseCode()!=200)throw new IOException();MessageDigest digest=MessageDigest.getInstance("SHA-256");long count=0,last=0,lastUpdate=0;
                try(InputStream in=c.getInputStream();FileOutputStream out=new FileOutputStream(partial)){byte[] block=new byte[65536];for(int n;(n=in.read(block))!=-1;){if(token!=serial)throw new InterruptedException();count+=n;if(count>MODEL_BYTES)throw new IOException();out.write(block,0,n);digest.update(block,0,n);if(count-last>1048576&&SystemClock.elapsedRealtime()-lastUpdate>=500){last=count;lastUpdate=SystemClock.elapsedRealtime();long progress=count;main.post(()->{if(token==serial){downloaded=progress;notifyChanged();}});}}out.getFD().sync();}
                c.disconnect();if(token!=serial||count!=MODEL_BYTES||!SHA.equals(hex(digest.digest())))throw new IOException();if(!partial.renameTo(model))throw new IOException();write(marker,new JSONObject().put("sha256",SHA));ok=true;
            }catch(Exception ignored){}finally{partial.delete();connection=null;refreshSizes();}
            boolean success=ok;main.post(()->{if(token!=serial)return;downloading=false;ready=success;downloaded=success?MODEL_BYTES:0;status=success?"Modelo verificado. Toque em Analisar letras.":"Download incompleto ou inválido. Tente novamente; nenhum modelo foi ativado.";notifyChanged();});
        });
    }
    void analyze(JSONArray tracks,int days){if(downloading)return;cancel();period=days;summary=sources="";if(!ready){status="Baixe o modelo de 468,6 MiB para interpretar as letras localmente.";notifyChanged();return;}if(tracks.length()==0){status="Não há músicas ouvidas neste período.";notifyChanged();return;}busy=true;int token=serial;status="Preparando até 8 letras do período…";notifyChanged();
        ArrayList<JSONObject> selected=new ArrayList<>();for(Object v:MusicStore.values(tracks))if(v instanceof JSONObject&&selected.size()<8)selected.add((JSONObject)v);
        worker.execute(()->{ArrayList<String> texts=new ArrayList<>();StringBuilder labels=new StringBuilder();for(JSONObject t:selected){if(token!=serial)return;String text=read(lyricFile(t)).optString("text");if(text.isEmpty()&&System.currentTimeMillis()>=cooldown)try{if(!awaitNetwork(token))return;Thread.sleep(1250);if(!awaitNetwork(token))return;String url=WebRequest.params("https://lrclib.net/api/get","track_name",t.optString("title"),"artist_name",t.optString("artist"));if(!t.optString("album").isEmpty())url=WebRequest.params(url,"album_name",t.optString("album"));if(t.optInt("durationMs")>0)url=WebRequest.params(url,"duration",String.valueOf(Math.round(t.optInt("durationMs")/1000.)));HttpURLConnection c=open(url,6000);int code=c.getResponseCode();if(code==429||code==503)cooldown=System.currentTimeMillis()+60000;
                    if(code==200)try(InputStream in=c.getInputStream();ByteArrayOutputStream body=new ByteArrayOutputStream()){byte[] block=new byte[8192];for(int n;(n=in.read(block))!=-1;){if(token!=serial)return;if(body.size()+n>262144)throw new IOException();body.write(block,0,n);}JSONObject o=new JSONObject(body.toString("UTF-8"));if(IndieCatalog.normalize(o.optString("trackName")).equals(IndieCatalog.normalize(t.optString("title")))&&IndieCatalog.normalize(o.optString("artistName")).equals(IndieCatalog.normalize(t.optString("artist")))&&(t.optInt("durationMs")<=0||Math.abs(o.optDouble("duration")*1000-t.optInt("durationMs"))<=5000)){text=o.optString("plainLyrics","");if(text.equals("null"))text="";if(text.isEmpty())for(Object line:MusicStore.values(MusicStore.parseLyrics(o.optString("syncedLyrics"))))text+=((JSONObject)line).optString("text")+"\n";}}
                    c.disconnect();connection=null;
                }catch(Exception ignored){}
                if(text.trim().length()>=20){try{write(lyricFile(t),new JSONObject().put("text",text.substring(0,Math.min(6000,text.length()))));}catch(Exception ignored){}texts.add(excerpt(text));if(labels.length()>0)labels.append(" • ");labels.append(t.optString("title")).append(" — ").append(t.optString("artist"));}
            }trim(new File(root,"lyrics"),128);refreshSizes();String key=hash("tonal-stories-v2|"+SHA+days+new JSONArray(texts));JSONObject old=read(new File(root,"summaries/"+key+".json"));String cached=old.optString("summary"),source=labels.toString();main.post(()->{if(token!=serial)return;sources=source;prepared=texts.toArray(new String[0]);cacheKey=key;if(texts.isEmpty()){busy=false;status="Nenhuma letra correspondente encontrada. Abra ou importe letras e tente novamente.";notifyChanged();return;}if(!cached.isEmpty()){summary=cached;busy=false;status="Resumo local em cache • "+texts.size()+" letras.";notifyChanged();return;}run(token);});
        });
    }
    private static String excerpt(String text){return text.length()<=800?text:text.substring(0,260)+"\n[…]\n"+text.substring(text.length()/2,text.length()/2+260)+"\n[…]\n"+text.substring(text.length()-260);}
    private void run(int token){
        worker.execute(()->{boolean available=enoughMemory();main.post(()->{if(token!=serial||!busy)return;if(!available){status="Aguardando memória livre para preservar a reprodução. Você pode cancelar.";notifyChanged();main.postDelayed(()->{if(token==serial&&busy)run(token);},5000);}else bindModel(token);});});
    }
    private void bindModel(int token){status="Interpretando "+prepared.length+" letras no aparelho…";notifyChanged();Messenger result=new Messenger(new Handler(Looper.getMainLooper(),message->{if(token!=serial||message.what!=StoryInferenceService.RESULT)return true;String output=message.getData().getString("output","");unbind();busy=false;if(output.startsWith("ERROR:")){status=output.substring(6);}else if(output.trim().length()<20){status="O modelo não produziu um resumo útil.";}else{summary=output.trim();status="Resumo experimental gerado no aparelho • "+prepared.length+" letras.";String value=summary,source=sources,key=cacheKey;worker.execute(()->{try{write(new File(root,"summaries/"+key+".json"),new JSONObject().put("summary",value).put("sources",source));trim(new File(root,"summaries"),20);refreshSizes();}catch(Exception ignored){}});}notifyChanged();return true;}));
        service=new ServiceConnection(){public void onServiceConnected(ComponentName name,IBinder binder){if(token!=serial)return;try{Message request=Message.obtain(null,StoryInferenceService.GENERATE);request.replyTo=result;Bundle data=new Bundle();data.putStringArray("texts",prepared);request.setData(data);new Messenger(binder).send(request);}catch(Exception error){unbind();busy=false;status="Não foi possível iniciar a análise local.";notifyChanged();}}public void onServiceDisconnected(ComponentName name){if(token==serial&&busy){unbind();busy=false;status="A análise foi interrompida. A reprodução continua disponível.";notifyChanged();}}};
        try{bound=app.bindService(new Intent(app,StoryInferenceService.class),service,Context.BIND_AUTO_CREATE|Context.BIND_NOT_FOREGROUND);}catch(Exception ignored){bound=false;}if(!bound){busy=false;status="Componente de análise indisponível. Atualize o app.";notifyChanged();}else main.postDelayed(timeout,105000);
    }
}
