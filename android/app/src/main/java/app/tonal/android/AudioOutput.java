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
    private VolumeShaper shaper;
    private Handler control;
    private volatile long writtenFrames;
    private long pauseQueuedFrames,lastPauseFadeMs;
    private volatile boolean paused;
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
        config=AudioConfig.load(prefs(),routeKey);applyDsp();
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
        if(!next.equals(routeKey)){routeKey=next;config=AudioConfig.load(prefs(),next);lastUnderruns=track.getUnderrunCount();applyDsp();setBuffer(true);}
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
        track=next;writtenFrames=0;lastUnderruns=0;next.addOnRoutingChangedListener(router->refreshRoute(),main);
        if(preferred!=null)next.setPreferredDevice(preferred);
        if(control==null){HandlerThread worker=new HandlerThread("TonalAudioControl",android.os.Process.THREAD_PRIORITY_AUDIO);worker.start();control=new Handler(worker.getLooper());}
        setBuffer(true);shaper=next.createVolumeShaper(envelope(true,30));
        shaper.apply(VolumeShaper.Operation.PLAY);
        paused=false;next.play();main.post(this::refreshRoute);
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
        AudioTrack current;float[] rendered;long start=System.nanoTime();
        synchronized(this){current=track;if(current==null)return;rendered=dsp.render(samples);}
        visualize(rendered);
        long elapsed=System.nanoTime()-start;dspNanos.addAndGet(elapsed);blocks.incrementAndGet();maxDspNanos.accumulateAndGet(elapsed,Math::max);
        if(elapsed>samples.length*1_000_000_000L/(Math.max(1,channels)*rate))overBudgetBlocks.incrementAndGet();
        int offset=0,retries=0;
        while(offset<rendered.length&&current==track){
            int count;
            try{count=BuildConfig.DEBUG&&testDeadObject.getAndSet(false)?AudioTrack.ERROR_DEAD_OBJECT:current.write(rendered,offset,rendered.length-offset,AudioTrack.WRITE_BLOCKING);}catch(IllegalStateException error){if(current!=track)return;throw error;}
            if(count==AudioTrack.ERROR_DEAD_OBJECT&&retries++<2){synchronized(this){if(current!=track)return;boolean wasPaused=paused;releaseTrack(false);createTrack();recoveries++;current=track;if(wasPaused)pause();}continue;}
            if(count<0){if(current!=track)return;throw new IllegalStateException("Saída de áudio falhou ("+count+")");}
            if(count==0)break;offset+=count;frames.addAndGet(count/2);if(current==track)writtenFrames+=count/2;adapt(current);
        }
    }
    synchronized void pause(){
        if(track==null||paused)return;paused=true;int serial=++transition;
        pauseQueuedFrames=queuedFrames(track);lastPauseFadeMs=30;
        shaper.apply(VolumeShaper.Operation.REVERSE);
        control.postDelayed(()->{synchronized(AudioOutput.this){if(track!=null&&serial==transition&&paused)track.pause();}},50);
    }
    private static VolumeShaper.Configuration envelope(boolean up,long duration){return new VolumeShaper.Configuration.Builder().setDuration(duration).setCurve(new float[]{0,1},up?new float[]{0,1}:new float[]{1,0}).build();}
    private long queuedFrames(AudioTrack current){long ahead=writtenFrames-Integer.toUnsignedLong(current.getPlaybackHeadPosition());return ahead<0?0:Math.min(current.getBufferCapacityInFrames(),ahead&0xffffffffL);}
    synchronized void resume(){if(track==null||!paused)return;paused=false;++transition;track.play();shaper.apply(VolumeShaper.Operation.PLAY);}
    void finish(){
        synchronized(this){if(track==null||control==null)return;}
        write(new float[(rate*30/1000+256)*channels]);
        synchronized(this){int serial=transition;control.postDelayed(()->{synchronized(AudioOutput.this){if(serial==transition)pause();}},Math.max(20,bufferFrames*1000L/rate+30));}
    }
    private static void closeTrack(AudioTrack current,VolumeShaper envelope){
        try{current.stop();}catch(IllegalStateException ignored){}if(envelope!=null)try{envelope.close();}catch(IllegalStateException ignored){}current.release();
    }
    private void releaseTrack(boolean fade){
        ++transition;AudioTrack current=track;VolumeShaper oldEnvelope=shaper;track=null;
        if(current!=null){
            previousUnderruns+=current.getUnderrunCount();boolean retiring=false;
            if(fade&&!paused&&oldEnvelope!=null){try{oldEnvelope.apply(VolumeShaper.Operation.REVERSE);control.postDelayed(()->closeTrack(current,oldEnvelope),50);retiring=true;}catch(IllegalStateException ignored){}}
            if(!retiring)closeTrack(current,oldEnvelope);
        }
        shaper=null;paused=false;
    }
    synchronized void release(){releaseTrack(true);dsp.close();}
    synchronized boolean isPlaying(){return track!=null&&track.getPlayState()==AudioTrack.PLAYSTATE_PLAYING;}
    synchronized int underruns(){return previousUnderruns+(track==null?0:track.getUnderrunCount());}
    AudioDeviceInfo[] devices(){return manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);}
    synchronized boolean prefer(AudioDeviceInfo device){if(device!=null&&device.getType()==AudioDeviceInfo.TYPE_BLUETOOTH_SCO)return false;preferred=device;return track==null||track.setPreferredDevice(device);}
    String routeName(){AudioDeviceInfo current=route;return current==null?"Saída padrão do Android (inicie uma música)":current.getProductName()+" • "+(bluetooth(current)?"Bluetooth":headphones(current)?"Fones":"Saída de mídia");}
    synchronized String diagnostics(){return routeName()+"\n"+(track==null?"Saída parada":rate+" Hz • PCM float estéreo • buffer do app "+Math.round(bufferFrames*1000f/rate)+" ms • falhas de buffer "+underruns()+" • recuperações "+recoveries)+"\nCodec e atraso total Bluetooth: controlados pelo Android e pelo receptor.";}
    void testDeadObject(){if(BuildConfig.DEBUG)testDeadObject.set(true);}
    synchronized org.json.JSONObject testSnapshot()throws org.json.JSONException{
        return new org.json.JSONObject().put("frames",frames.get()).put("blocks",blocks.get()).put("dspNanos",dspNanos.get()).put("maxDspNanos",maxDspNanos.get()).put("overBudgetBlocks",overBudgetBlocks.get()).put("underruns",underruns()).put("recoveries",recoveries).put("bufferFrames",bufferFrames).put("rate",rate).put("channels",channels).put("routeKey",routeKey).put("route",routeName()).put("hrtfRequested",dsp.settings.values[16]!=0).put("eq",config.eq).put("enabled",config.enabled).put("binauralSource",binauralSource).put("paused",paused).put("volume",shaper==null?1:shaper.getVolume()).put("pauseQueuedFrames",pauseQueuedFrames).put("pauseFadeMs",lastPauseFadeMs);
    }
}
