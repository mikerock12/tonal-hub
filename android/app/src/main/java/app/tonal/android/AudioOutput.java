package app.tonal.android;

import android.content.*;
import android.media.*;
import android.os.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicBoolean;

/** One MEDIA/MUSIC AudioTrack, adaptive buffering and app-only DSP before Bluetooth encoding. */
final class AudioOutput {
    private volatile AudioTrack track;
    private final Object trackLock=new Object();
    private VolumeShaper shaper;
    private Handler control;
    private volatile long writtenFrames;
    private long pauseQueuedFrames,lastPauseFadeMs;
    private volatile boolean paused;
    // Set while suspended when the queued tail stopped belonging to what plays next.
    private boolean staleTail;
    private int staleFlushes;private long staleFrames;
    private volatile long lastTransition;
    /** When the output last paused, resumed or opened; gaps right after that are expected. */
    long lastTransition(){return lastTransition;}
    private final Handler main=new Handler(Looper.getMainLooper());
    private int transition,rate=44100,channels=2,lastUnderruns,previousUnderruns,bufferFrames,recoveries,minimumStableFrames;
    private long lastAdapt;
    private Context context;private AudioManager manager;private AudioDeviceInfo preferred;
    private volatile AudioDeviceInfo route;
    volatile String routeKey="default";
    volatile AudioConfig config=new AudioConfig();
    volatile boolean localSource,binauralSource;
    volatile boolean visualizerActive;
    volatile float[] waveform=new float[0],spectrum=new float[0];
    private long lastVisual;
    private void visualize(float[] pcm){
        long now=SystemClock.elapsedRealtime();if(!visualizerActive){if(waveform.length>0){waveform=new float[0];spectrum=new float[0];}return;}if(now-lastVisual<100||pcm.length<4)return;lastVisual=now;
        double[] mono=new double[256];for(int i=0;i<256;i++){int row=Math.min(pcm.length/2-1,i);double value=(pcm[row*2]+pcm[row*2+1])*.5;mono[i]=Double.isFinite(value)?value:0;}
        float[] wave=new float[64],spec=new float[16];for(int i=0;i<64;i++)wave[i]=(float)Math.max(-1,Math.min(1,mono[i*4]));
        for(int b=0;b<16;b++){int bin=Math.max(1,Math.min(128,(int)Math.round(Math.pow(128,b/15.))));double re=0,im=0;for(int i=0;i<256;i++){double window=.5-.5*Math.cos(2*Math.PI*i/255),angle=2*Math.PI*bin*i/256;re+=mono[i]*window*Math.cos(angle);im-=mono[i]*window*Math.sin(angle);}double magnitude=Math.hypot(re,im)*4/256;spec[b]=(float)Math.max(0,Math.min(1,(20*Math.log10(Math.max(.000001,magnitude))+60)/60));}waveform=wave;spectrum=spec;
    }
    volatile double localLufs=-70,localPeak,replayGain=Double.NaN,replayPeak;
    final Dsp dsp=new Dsp();
    final AtomicLong frames=new AtomicLong(),dspNanos=new AtomicLong(),blocks=new AtomicLong();
    final AtomicLong maxDspNanos=new AtomicLong(),overBudgetBlocks=new AtomicLong();
    private final AtomicBoolean testDeadObject=new AtomicBoolean();
    private String preferenceName="settings";
    synchronized void initialize(Context ctx){initialize(ctx,"settings");}
    synchronized void initialize(Context ctx,String preferences){
        if(context!=null)return;context=ctx.getApplicationContext();manager=context.getSystemService(AudioManager.class);
        preferenceName=preferences;
        config=AudioConfig.load(prefs(),routeKey);applyDsp();if(preferences.equals("settings"))Transitions.load(prefs());
        manager.registerAudioDeviceCallback(new AudioDeviceCallback(){
            @Override public void onAudioDevicesAdded(AudioDeviceInfo[] devices){refreshRoute();}
            @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] devices){synchronized(AudioOutput.this){if(preferred!=null)for(AudioDeviceInfo d:devices)if(d.getId()==preferred.getId())preferred=null;}refreshRoute();}
        },main);
    }
    SharedPreferences prefs(){return context.getSharedPreferences(preferenceName,0);}
    static boolean bluetooth(AudioDeviceInfo d){return d!=null&&(d.getType()==AudioDeviceInfo.TYPE_BLUETOOTH_A2DP||d.getType()==AudioDeviceInfo.TYPE_BLE_HEADSET||d.getType()==AudioDeviceInfo.TYPE_BLE_SPEAKER||d.getType()==AudioDeviceInfo.TYPE_BLE_BROADCAST);}
    static boolean headphones(AudioDeviceInfo d){return d!=null&&(d.getType()==AudioDeviceInfo.TYPE_WIRED_HEADPHONES||d.getType()==AudioDeviceInfo.TYPE_WIRED_HEADSET||d.getType()==AudioDeviceInfo.TYPE_USB_HEADSET||d.getType()==AudioDeviceInfo.TYPE_BLE_HEADSET);}
    static String key(AudioDeviceInfo d){
        if(d==null)return "default";
        try{String identity=d.getType()+"|"+d.getAddress()+"|"+d.getProductName();byte[] hash=MessageDigest.getInstance("SHA-256").digest(identity.getBytes(StandardCharsets.UTF_8));StringBuilder result=new StringBuilder();for(int i=0;i<12;i++)result.append(String.format(java.util.Locale.ROOT,"%02x",hash[i]&255));return result.toString();}
        catch(Exception ignored){return "type"+d.getType();}
    }
    private synchronized void refreshRoute(){
        AudioDeviceInfo actual=track==null?null:track.getRoutedDevice();
        // Until playback starts, no preferred device is reported as an actual route.
        if(actual==null)return;
        route=actual;String next=key(actual);
        if(!next.equals(routeKey)){routeKey=next;config=AudioConfig.load(prefs(),next);lastUnderruns=track.getUnderrunCount();applyDsp();setBuffer(true);cancelTransition();}
    }
    synchronized void updateConfig(AudioConfig next,String key){if(!routeKey.equals(key))return;config=next;next.save(prefs(),key);applyDsp();setBuffer(true);}
    private void applyDsp(){
        float gain=0;
        if(localSource&&config.normalization){
            if(Double.isFinite(replayGain))gain=(float)Math.max(-24,Math.min(12,replayGain+config.target+18));
            else if(localLufs>-69&&Double.isFinite(localLufs))gain=(float)Math.max(-24,Math.min(12,config.target-localLufs));
            double peak=Math.max(localPeak,replayPeak);if(peak>0)gain=(float)Math.min(gain,config.ceiling-20*Math.log10(peak));
        }
        dsp.settings=config.snapshot(gain,headphones(route));
    }
    synchronized void source(boolean local,boolean binaural,double lufs,double peak,double replay,double rgPeak){localSource=local;binauralSource=binaural;localLufs=lufs;localPeak=peak;replayGain=replay;replayPeak=rgPeak;applyDsp();}
    synchronized void open(int sampleRate){open(sampleRate,2);}
    synchronized void startStereo(int sampleRate){
        if(track!=null&&rate==sampleRate&&channels==2)resume();
        else open(sampleRate,2);
    }
    synchronized void open(int sampleRate,int channelCount){
        release();rate=sampleRate;channels=channelCount;dsp.reset(rate,channels,binauralSource);applyDsp();createTrack();
    }
    private void createTrack(){
        int minimum=AudioTrack.getMinBufferSize(rate,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_FLOAT);
        if(minimum<0)throw new IllegalStateException("Formato AudioTrack não suportado");
        // Keep a full 30 ms fade plus mixer/scheduling margin after the producer
        // stops. Android's minimum includes the device's own burst requirements.
        minimumStableFrames=Math.max(rate*80/1000,minimum/8+rate*40/1000);
        AudioAttributes.Builder attributes=new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC);
        // Original stays original; HRTF/IAMF already render to stereo in our native plugin.
        if(Build.VERSION.SDK_INT>=32)attributes.setSpatializationBehavior(AudioAttributes.SPATIALIZATION_BEHAVIOR_NEVER);
        AudioTrack.Builder builder=new AudioTrack.Builder().setAudioAttributes(attributes.build())
            .setAudioFormat(new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_FLOAT).setSampleRate(rate).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
            .setBufferSizeInBytes(Math.max(minimum,rate*8*350/1000)).setTransferMode(AudioTrack.MODE_STREAM);
        if(config.buffer==2&&route!=null&&!bluetooth(route))builder.setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY);
        AudioTrack next=builder.build();if(next.getState()!=AudioTrack.STATE_INITIALIZED){next.release();throw new IllegalStateException("Falha ao abrir saída de áudio");}
        synchronized(trackLock){
            track=next;writtenFrames=0;lastUnderruns=0;lastTransition=SystemClock.elapsedRealtime();next.addOnRoutingChangedListener(router->refreshRoute(),main);
            if(preferred!=null)next.setPreferredDevice(preferred);
            if(control==null){HandlerThread worker=new HandlerThread("TonalAudioControl",android.os.Process.THREAD_PRIORITY_AUDIO);worker.start();control=new Handler(worker.getLooper());}
            setBuffer(true);shaper=next.createVolumeShaper(envelope(true,30));
            shaper.apply(VolumeShaper.Operation.PLAY);
            paused=false;next.play();
        }
        main.post(this::refreshRoute);
    }
    private void setBuffer(boolean reset){
        AudioTrack current=track;if(current==null)return;
        int requested=reset?rate*AudioConfig.bufferMillis(config.buffer,bluetooth(route))/1000:bufferFrames;
        if(reset)requested=Math.max(requested,minimumStableFrames+rate*(config.buffer==1?60:config.buffer==0?20:0)/1000);
        requested=Math.max(minimumStableFrames,requested);
        int actual=current.setBufferSizeInFrames(Math.min(current.getBufferCapacityInFrames(),Math.max(1,requested)));
        if(actual>0){bufferFrames=actual;if(Build.VERSION.SDK_INT>=31)current.setStartThresholdInFrames(actual);}
    }
    private synchronized void adapt(AudioTrack current){
        if(current!=track)return;long now=SystemClock.elapsedRealtime();if(now-lastAdapt<1000)return;lastAdapt=now;
        int count=current.getUnderrunCount();if(count>lastUnderruns){bufferFrames=AudioConfig.growBuffer(bufferFrames,rate,current.getBufferCapacityInFrames());setBuffer(false);}lastUnderruns=count;
    }
    int sampleRate(){return rate;}
    void write(float[] samples){
        long counted=trackFrames;if(counted>=0)trackFrames=counted+samples.length/Math.max(1,channels);
        if(mixActive&&channels==2){if(writeTransition(samples))return;}
        float[] rendered=render(samples);
        if(rendered!=null)output(rendered,0,true);
    }
    private float[] render(float[] samples){
        float[] rendered;long start=System.nanoTime();
        synchronized(this){if(track==null)return null;rendered=dsp.render(samples);}
        visualize(rendered);
        long elapsed=System.nanoTime()-start;dspNanos.addAndGet(elapsed);blocks.incrementAndGet();maxDspNanos.accumulateAndGet(elapsed,Math::max);
        if(elapsed>samples.length*1_000_000_000L/(Math.max(1,channels)*rate))overBudgetBlocks.incrementAndGet();
        return rendered;
    }
    /** Escreve a partir de `offset`; retorna quantas amostras saíram. trackLock serializa só o dispositivo, nunca o mixer. */
    private int output(float[] rendered,int offset,boolean blocking){
        AudioTrack current=track;if(current==null)return rendered.length-offset;
        int start=offset,retries=0;
        while(offset<rendered.length&&current==track){
            int count;
            try{synchronized(trackLock){if(current!=track)return rendered.length-start;count=BuildConfig.DEBUG&&testDeadObject.getAndSet(false)?AudioTrack.ERROR_DEAD_OBJECT:current.write(rendered,offset,rendered.length-offset,blocking?AudioTrack.WRITE_BLOCKING:AudioTrack.WRITE_NON_BLOCKING);}}catch(IllegalStateException error){if(current!=track)return rendered.length-start;throw error;}
            if(count==AudioTrack.ERROR_DEAD_OBJECT&&retries++<2){synchronized(this){if(current!=track)return rendered.length-start;boolean wasPaused=paused;releaseTrack(false);createTrack();recoveries++;current=track;if(wasPaused)pause();}continue;}
            if(count<0){if(current!=track)return rendered.length-start;throw new IllegalStateException("Saída de áudio falhou ("+count+")");}
            if(count==0)break;offset+=count;frames.addAndGet(count/2);if(current==track)writtenFrames+=count/2;adapt(current);
            if(!blocking&&offset<rendered.length)break;
        }
        return offset-start;
    }

    // ------------------------------------------------------------ transições
    // O motor entrega PCM e marcadores na mesma fila SPSC limitada. Só a thread
    // de saída empurra esses eventos ao mixer: onPcm nunca segura mixLock nem
    // espera um wake-up temporizado do DSP. Se a fila encher, a contrapressão
    // fica no produtor do motor e é cancelada pelo epoch, sem perder PCM aceito.
    private final Object mixLock=new Object();
    private final TransitionInputQueue mixInput=new TransitionInputQueue(256);
    private long mixer;
    private volatile boolean mixActive;
    private volatile int latencyFrames;
    private volatile long mixEpoch;
    private volatile boolean mixShutdown;
    private Thread writer;
    private final float[] mixOut=new float[1024*2];
    private long transitionIds;
    private final AtomicLong transitionQueueWaits=new AtomicLong(),transitionQueueWaitNanos=new AtomicLong(),transitionQueueMaxSize=new AtomicLong(),transitionDirectFallbacks=new AtomicLong();
    /** Começos de faixa marcados pelo librespot: identifica cada ocorrência no PCM. */
    volatile long trackSequence;
    private volatile long trackFrames=-1;
    private volatile int trackStartMs;
    volatile boolean markersSeen;
    volatile int markerBoundaries;
    void boundary(int positionMs){
        markersSeen=true;trackStartMs=positionMs;trackFrames=0;
        boolean resumed=positionMs>1000;
        if(!resumed)trackSequence++;
        long epoch=mixEpoch;
        if(mixActive)mixInput.put(null,resumed?TransitionInputQueue.RESUME:TransitionInputQueue.BOUNDARY,0,epoch,()->epoch!=mixEpoch);
    }
    /** Seek ou outra escolha: a contagem desde o marcador deixa de valer até o próximo. */
    void forgetTrackPosition(){trackFrames=-1;}
    /** Posição da faixa no PCM já entregue pelo motor; -1 quando desconhecida. */
    long trackPositionMs(){long counted=trackFrames;return counted<0?-1:trackStartMs+counted*1000L/Math.max(1,rate);}
    /** Retorna o id do pedido aceito pelo mixer (0: recusado). O resultado sai em transitionTrace(). */
    long armTransition(int effect,long upcomingMs,long outroMs,long introMs){
        if(!TransitionNative.READY)return 0;
        int current;
        synchronized(this){if(track==null||channels!=2||localSource||paused)return 0;current=rate;}
        synchronized(mixLock){
            if(mixer!=0&&!mixActive&&TransitionNative.rate(mixer)!=current){TransitionNative.destroy(mixer);mixer=0;}
            if(mixer==0)mixer=TransitionNative.create(current);
            if(mixer==0)return 0;
            long id=++transitionIds;
            if(!TransitionNative.arm(mixer,effect,upcomingMs*current/1000,outroMs*current/1000,introMs*current/1000,true,id))return 0;
            ++mixEpoch;mixShutdown=false;mixActive=true;latencyFrames=(int)TransitionNative.latency(mixer);
            if(writer==null||!writer.isAlive()){writer=new Thread(this::writerLoop,"TonalTransitionOut");writer.start();}
            mixLock.notifyAll();mixInput.wakeAll();
            return id;
        }
    }
    /** O que a última transição realmente fez (TransitionNative.T_*); null sem mixer. */
    long[] transitionTrace(){synchronized(mixLock){return mixer==0?null:TransitionNative.trace(mixer);}}
    void cancelTransition(){
        synchronized(mixLock){++mixEpoch;if(mixer!=0)TransitionNative.reset(mixer);mixActive=false;latencyFrames=0;mixLock.notifyAll();}
        mixInput.wakeAll();
    }
    int transitionState(){synchronized(mixLock){return mixer==0||!mixActive?0:TransitionNative.state(mixer);}}
    /** Áudio já entregue pelo motor e ainda não tocado por causa da transição. */
    int transitionLatencyMs(){return mixActive?(int)(latencyFrames*1000L/Math.max(1,rate)):0;}
    /** Motor parou de entregar no fim natural: a transição continua tocando o que já leu. */
    boolean continuesThroughEngineStop(){
        if(!mixActive)return false;
        long epoch=mixEpoch;
        return mixInput.put(null,TransitionInputQueue.END_CAPTURE,0,epoch,()->epoch!=mixEpoch);
    }
    private boolean writeTransition(float[] samples){
        if(!mixActive)return false;
        long epoch=mixEpoch;
        int queued=mixInput.size();transitionQueueMaxSize.accumulateAndGet(queued,Math::max);
        boolean full=queued>=mixInput.capacity(),interrupted=false;
        if(full)transitionQueueWaits.incrementAndGet();
        long started=full?System.nanoTime():0;
        boolean accepted=false;
        while(!accepted&&epoch==mixEpoch){
            accepted=mixInput.put(samples,TransitionInputQueue.PCM,0,epoch,()->epoch!=mixEpoch);
            if(!accepted&&epoch==mixEpoch){interrupted=true;Thread.interrupted();}
        }
        if(full)transitionQueueWaitNanos.addAndGet(System.nanoTime()-started);
        if(interrupted)Thread.currentThread().interrupt();
        return true;
    }
    private void writerLoop(){
        try{android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);}catch(SecurityException ignored){}
        TransitionInputQueue.Entry pending=null;
        boolean drained=false;
        try{
            while(true){
                if(pending==null)pending=mixInput.peek();
                if(pending!=null&&pending.epoch!=mixEpoch){mixInput.release(pending);pending=null;continue;}
                long epoch=mixEpoch;
                int got=0;
                boolean nativeEmpty=false;
                synchronized(mixLock){
                    if(mixer==0)break;
                    if(pending!=null&&pending.epoch==mixEpoch){
                        if(pending.event!=TransitionInputQueue.PCM){
                            if(mixActive){
                                if(pending.event==TransitionInputQueue.RESUME)TransitionNative.resumeCapture(mixer);
                                else if(pending.event==TransitionInputQueue.END_CAPTURE)TransitionNative.endCapture(mixer);
                                else{if(TransitionNative.state(mixer)==1)markerBoundaries++;TransitionNative.boundary(mixer);}
                            }
                            mixInput.release(pending);pending=null;mixLock.notifyAll();continue;
                        }
                        if(mixActive&&TransitionNative.wantsInput(mixer)){
                            int used=TransitionNative.push(mixer,pending.samples,pending.offset,pending.samples.length-pending.offset);
                            if(used>0){pending.offset+=used;latencyFrames=(int)TransitionNative.latency(mixer);}
                            if(pending.offset>=pending.samples.length){mixInput.release(pending);pending=null;}
                        }
                    }
                    if(mixActive){
                        got=TransitionNative.pullInto(mixer,mixOut,mixOut.length/2);
                        latencyFrames=(int)TransitionNative.latency(mixer);
                        nativeEmpty=got==0&&TransitionNative.state(mixer)==0;
                        if(nativeEmpty&&pending==null&&mixInput.isEmpty()){mixActive=false;latencyFrames=0;drained=true;mixLock.notifyAll();}
                    }else if(mixShutdown&&pending==null&&mixInput.isEmpty())break;
                }
                if(drained){drained=false;if(this==NativeBridge.audio)NativeBridge.onTransitionDrained();}
                if(got>0){
                    float[] chunk=got*2==mixOut.length?mixOut:java.util.Arrays.copyOf(mixOut,got*2);
                    synchronized(mixLock){while(paused&&mixActive&&epoch==mixEpoch)mixLock.wait();}
                    if(epoch!=mixEpoch||!mixActive)continue;
                    float[] rendered=render(chunk);
                    if(rendered!=null&&epoch==mixEpoch&&mixActive)output(rendered,0,true);
                    continue;
                }
                if((nativeEmpty||!mixActive)&&pending!=null){
                    // O mixer drenou antes deste pacote já aceito: preserva a ordem
                    // pelo próprio writer, sem devolver PCM antigo depois de um cancel.
                    TransitionInputQueue.Entry item=pending;
                    float[] rest=item.offset==0?item.samples:java.util.Arrays.copyOfRange(item.samples,item.offset,item.samples.length);
                    float[] rendered=render(rest);
                    if(rendered!=null&&epoch==mixEpoch&&!mixShutdown){transitionDirectFallbacks.incrementAndGet();output(rendered,0,true);}
                    mixInput.release(item);pending=null;continue;
                }
                synchronized(mixLock){if(mixShutdown&&pending==null&&mixInput.isEmpty())break;}
                if(pending==null&&mixInput.isEmpty())mixInput.awaitWork();
                else Thread.yield();
            }
        }catch(InterruptedException interrupted){Thread.currentThread().interrupt();}
        finally{
            synchronized(mixLock){if(Thread.currentThread()==writer)writer=null;mixLock.notifyAll();}
            mixInput.wakeAll();
        }

    }
    synchronized void pause(){
        if(track==null||paused)return;paused=true;int serial=++transition;lastTransition=SystemClock.elapsedRealtime();
        synchronized(mixLock){mixLock.notifyAll();}mixInput.wakeAll();
        pauseQueuedFrames=queuedFrames(track);lastPauseFadeMs=30;
        shaper.apply(VolumeShaper.Operation.REVERSE);
        control.postDelayed(()->{synchronized(AudioOutput.this){AudioTrack current=track;if(current!=null&&serial==transition&&paused)synchronized(trackLock){if(current==track)current.pause();}}},50);
    }
    private static VolumeShaper.Configuration envelope(boolean up,long duration){return new VolumeShaper.Configuration.Builder().setDuration(duration).setCurve(new float[]{0,1},up?new float[]{0,1}:new float[]{1,0}).build();}
    private long queuedFrames(AudioTrack current){long ahead=writtenFrames-Integer.toUnsignedLong(current.getPlaybackHeadPosition());return ahead<0?0:Math.min(current.getBufferCapacityInFrames(),ahead&0xffffffffL);}
    /**
     * The next audio is not a continuation: another track or position was selected.
     * Pausing keeps the tail on purpose so a plain pause/resume stays seamless, but
     * after a selection that tail is a fragment of the previous song, heard as a
     * short tone right at the start of the new one.
     */
    synchronized void discontinuity(){if(paused)staleTail=true;}
    /** Evento efetivamente aplicado pelo motor: remove PCM antigo também sem pausar a nova seleção. */
    synchronized void transportDiscontinuity(){
        AudioTrack current=track;if(current==null)return;
        boolean resume=!paused;++transition;lastTransition=SystemClock.elapsedRealtime();
        staleFrames+=queuedFrames(current);staleFlushes++;staleTail=false;
        synchronized(trackLock){if(current!=track)return;current.pause();current.flush();writtenFrames=0;if(resume)current.play();}
        dsp.reset(rate,channels,binauralSource);applyDsp();
        synchronized(mixLock){mixLock.notifyAll();}mixInput.wakeAll();
    }
    synchronized void resume(){
        if(track==null||!paused)return;paused=false;++transition;lastTransition=SystemClock.elapsedRealtime();
        synchronized(mixLock){mixLock.notifyAll();}mixInput.wakeAll();
        AudioTrack current=track;
        synchronized(trackLock){
            if(current!=track)return;
            if(staleTail){
                staleTail=false;staleFrames+=queuedFrames(current);staleFlushes++;
                current.pause();current.flush();writtenFrames=0;
            }
            current.play();
        }
        shaper.apply(VolumeShaper.Operation.PLAY);
    }
    void finish(){
        synchronized(this){if(track==null||control==null)return;}
        write(new float[(rate*30/1000+256)*channels]);
        synchronized(this){int serial=transition;control.postDelayed(()->{synchronized(AudioOutput.this){if(serial==transition)pause();}},Math.max(20,bufferFrames*1000L/rate+30));}
    }
    private void closeTrack(AudioTrack current,VolumeShaper envelope){
        synchronized(trackLock){try{current.stop();}catch(IllegalStateException ignored){}if(envelope!=null)try{envelope.close();}catch(IllegalStateException ignored){}current.release();}
    }
    private void releaseTrack(boolean fade){
        ++transition;AudioTrack current=track;VolumeShaper oldEnvelope=shaper;track=null;
        if(current!=null){
            previousUnderruns+=current.getUnderrunCount();boolean retiring=false;
            if(fade&&!paused&&oldEnvelope!=null){try{oldEnvelope.apply(VolumeShaper.Operation.REVERSE);control.postDelayed(()->closeTrack(current,oldEnvelope),50);retiring=true;}catch(IllegalStateException ignored){}}
            if(!retiring)closeTrack(current,oldEnvelope);
        }
        shaper=null;paused=false;staleTail=false;
    }
    synchronized void release(){mixShutdown=true;cancelTransition();releaseTrack(true);dsp.close();mixInput.wakeAll();}
    synchronized boolean isPlaying(){return track!=null&&track.getPlayState()==AudioTrack.PLAYSTATE_PLAYING;}
    synchronized int underruns(){return previousUnderruns+(track==null?0:track.getUnderrunCount());}
    AudioDeviceInfo[] devices(){return manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);}
    synchronized boolean prefer(AudioDeviceInfo device){if(device!=null&&device.getType()==AudioDeviceInfo.TYPE_BLUETOOTH_SCO)return false;preferred=device;return track==null||track.setPreferredDevice(device);}
    String routeName(){AudioDeviceInfo current=route;return current==null?"Saída padrão do Android (inicie uma música)":current.getProductName()+" • "+(bluetooth(current)?"Bluetooth":headphones(current)?"Fones":"Saída de mídia");}
    synchronized String diagnostics(){return routeName()+"\n"+(track==null?"Saída parada":rate+" Hz • PCM float estéreo • buffer do app "+Math.round(bufferFrames*1000f/rate)+" ms • falhas de buffer "+underruns()+" • recuperações "+recoveries)+"\n"+(!config.enabled?"Processamento desligado":("EQ "+(config.eq?"ligado":"desligado")+" • Graves "+config.bass+" dB • Compressor "+(config.compressor?"ligado":"desligado")+" • "+(binauralSource?"IAMF binaural":dsp.settings.values[16]!=0?"HRTF solicitada":config.spatial==1?"HRTF inativa neste perfil":"Estéreo original")))+"\nCodec e atraso total Bluetooth: controlados pelo Android e pelo receptor.";}
    synchronized long testQueuedFrames(){return track==null?0:queuedFrames(track);}
    void testDeadObject(){if(BuildConfig.DEBUG)testDeadObject.set(true);}
    synchronized org.json.JSONObject testSnapshot()throws org.json.JSONException{
        return new org.json.JSONObject().put("frames",frames.get()).put("blocks",blocks.get()).put("dspNanos",dspNanos.get()).put("maxDspNanos",maxDspNanos.get()).put("overBudgetBlocks",overBudgetBlocks.get()).put("underruns",underruns()).put("staleFlushes",staleFlushes).put("staleFrames",staleFrames).put("recoveries",recoveries).put("bufferFrames",bufferFrames).put("rate",rate).put("channels",channels).put("routeKey",routeKey).put("route",routeName()).put("hrtfRequested",dsp.settings.values[16]!=0).put("eq",config.eq).put("enabled",config.enabled).put("binauralSource",binauralSource).put("paused",paused).put("volume",shaper==null?1:shaper.getVolume()).put("pauseQueuedFrames",pauseQueuedFrames).put("pauseFadeMs",lastPauseFadeMs).put("transitionQueueSize",mixInput.size()).put("transitionQueueCapacity",mixInput.capacity()).put("transitionQueueWaits",transitionQueueWaits.get()).put("transitionQueueMaxSize",transitionQueueMaxSize.get()).put("transitionQueueWaitMs",transitionQueueWaitNanos.get()/1_000_000).put("transitionDirectFallbacks",transitionDirectFallbacks.get());
    }
}
