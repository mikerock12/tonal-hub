package app.tonal.android;

import android.app.*;
import android.content.*;
import android.media.*;
import android.media.session.*;
import android.os.*;
import java.io.File;
import java.util.function.Consumer;
import java.util.ArrayDeque;

public final class PlaybackService extends Service {
    private MediaSession media;
    private AudioManager audioManager;
    private AudioFocusRequest focus;
    private final Handler control=new Handler(Looper.getMainLooper());
    private final ArrayDeque<Long> restarts=new ArrayDeque<>();
    private boolean spotify,recovering,restartQueued,destroyed;
    private long startedAt;
    private String cacheName=SpotifyClient.ENGINE_CACHE;
    private boolean waitingNetwork;
    private android.net.ConnectivityManager connectivity;
    private final android.net.ConnectivityManager.NetworkCallback network=new android.net.ConnectivityManager.NetworkCallback(){@Override public void onCapabilitiesChanged(android.net.Network n,android.net.NetworkCapabilities c){if(c.hasCapability(android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED))control.post(()->{if(waitingNetwork&&!destroyed&&spotify){waitingNetwork=false;scheduleRecovery("Internet disponível novamente");}});}};
    private final BroadcastReceiver switchPlayer=new BroadcastReceiver(){@Override public void onReceive(Context c,Intent i){NativeBridge.command("pause");}};
    private final BroadcastReceiver noisy=new BroadcastReceiver(){
        @Override public void onReceive(Context context,Intent intent){NativeBridge.command("pause");}
    };
    private final Consumer<String> statusListener=this::update;
    // Media notification and lock screen: the playing track's title, artist,
    // album, duration and cover, read from the personal store once a second.
    private final java.util.concurrent.ExecutorService coverWorker=java.util.concurrent.Executors.newSingleThreadExecutor();
    private String status="Preparando áudio…",metadataKey="",coverUrl="";
    private android.graphics.Bitmap cover;
    private org.json.JSONObject shown=new org.json.JSONObject();
    private boolean statePlaying,stateLocal;private long statePosition=-1,stateAt;
    static void start(Context context,String action,String value){
        Intent intent=new Intent(context,PlaybackService.class).setAction(action);
        if(value!=null)intent.putExtra("value",value);
        context.startForegroundService(intent);
    }
    @Override public void onCreate(){
        super.onCreate();NativeBridge.audio.initialize(this);Diagnostics.initialize(this,"tonalify");AppTheme.apply(this);
        connectivity=getSystemService(android.net.ConnectivityManager.class);connectivity.registerDefaultNetworkCallback(network);
        NativeBridge.commandListener=command->{if(command.equals("stop")){spotify=false;cancelRecovery();}else if(command.equals("pause")||command.equals("toggle")&&!NativeBridge.intent.desired){cancelRecovery();}};
        IntentFilter switching=new IntentFilter("app.tonal.android.PAUSE_TONALIFY");
        if(Build.VERSION.SDK_INT>=33)registerReceiver(switchPlayer,switching,Context.RECEIVER_NOT_EXPORTED);else registerReceiver(switchPlayer,switching);
        control.postDelayed(watchdog,5000);control.post(historyTick);
        NotificationManager notifications=getSystemService(NotificationManager.class);
        notifications.createNotificationChannel(new NotificationChannel("playback","Reprodução",NotificationManager.IMPORTANCE_LOW));
        media=new MediaSession(this,"Tonalify");
        media.setCallback(new MediaSession.Callback(){
            @Override public void onPlay(){
                boolean locked=getSystemService(KeyguardManager.class).isKeyguardLocked();
                boolean allow=getSharedPreferences("settings",0).getBoolean("allowLockedMediaResume",false);
                synchronized(NativeBridge.intent){if(!NativeBridge.intent.allowMediaPlay(locked,allow)){Diagnostics.write("Retomada externa não solicitada bloqueada");return;}}
                if(audioManager.requestAudioFocus(focus)==AudioManager.AUDIOFOCUS_REQUEST_GRANTED)NativeBridge.command("play");
            }
            @Override public void onPause(){NativeBridge.command("pause");}
            @Override public void onSkipToNext(){NativeBridge.command("next");}
            @Override public void onSkipToPrevious(){NativeBridge.command("prev");}
            @Override public void onSeekTo(long position){if(!NativeBridge.intent.local)NativeBridge.command("seek|"+Math.max(0,position));}
            @Override public void onStop(){stopSelf();}
        });
        media.setActive(true);
        audioManager=getSystemService(AudioManager.class);
        focus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setOnAudioFocusChangeListener(change->{if(change<0)NativeBridge.command("pause");}).build();
        startForeground(1,notification("Preparando áudio…"));
        NativeBridge.listeners.add(statusListener);
        IntentFilter unplugged=new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        if(Build.VERSION.SDK_INT>=33)registerReceiver(noisy,unplugged,Context.RECEIVER_NOT_EXPORTED);
        else registerReceiver(noisy,unplugged);
    }
    private Notification notification(String text){
        status=text;
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder builder=new Notification.Builder(this,"playback").setSmallIcon(app.tonal.android.R.drawable.ic_tonal).setColor(Ui.BLUE)
            .setContentIntent(open).setOngoing(true).setStyle(new Notification.MediaStyle().setMediaSession(media.getSessionToken()));
        String title=shown.optString("title"),artist=shown.optString("artist");
        if(title.isEmpty())return builder.setContentTitle("Tonalify").setContentText(text).build();
        // Song and band first; engine messages (reconnecting, errors) stay visible below.
        boolean routine=text.equals("Reproduzindo")||text.equals("Pausado")||text.startsWith("Pronto");
        builder.setContentTitle(title).setContentText(artist.isEmpty()?text:artist).setSubText(routine?shown.optString("album"):text);
        if(cover!=null)builder.setLargeIcon(cover);
        return builder.build();
    }
    private void refreshMetadata(){
        org.json.JSONObject current=MusicStore.get(this).current;
        String key=current.optString("title")+"|"+current.optString("artist")+"|"+current.optString("album")+"|"+current.optString("image")+"|"+current.optInt("durationMs");
        if(key.equals(metadataKey))return;
        metadataKey=key;try{shown=new org.json.JSONObject(current.toString());}catch(org.json.JSONException e){shown=new org.json.JSONObject();}
        String image=shown.optString("image");
        if(!image.equals(coverUrl)){
            coverUrl=image;cover=null;
            if(!image.isEmpty())coverWorker.execute(()->{android.graphics.Bitmap art=Artwork.fetch(this,image);if(art!=null)control.post(()->{if(!destroyed&&image.equals(coverUrl)){cover=art;publishMetadata();}});});
        }
        publishMetadata();
    }
    private void publishMetadata(){
        MediaMetadata.Builder metadata=new MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE,shown.optString("title","Tonalify"))
            .putString(MediaMetadata.METADATA_KEY_ARTIST,shown.optString("artist"))
            .putString(MediaMetadata.METADATA_KEY_ALBUM,shown.optString("album"))
            .putLong(MediaMetadata.METADATA_KEY_DURATION,Math.max(0,shown.optInt("durationMs")));
        if(cover!=null)metadata.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART,cover);
        media.setMetadata(metadata.build());
        getSystemService(NotificationManager.class).notify(1,notification(status));
        statePosition=-1;
    }
    // Position for the notification bar: republished only when play state,
    // track or a jump (seek) changes it; the system extrapolates in between.
    private void publishState(long position,boolean local){
        boolean playing=NativeBridge.playing;long now=SystemClock.elapsedRealtime();
        long expected=statePosition+(statePlaying?now-stateAt:0);
        if(statePosition>=0&&playing==statePlaying&&local==stateLocal&&Math.abs(position-expected)<2000)return;
        statePlaying=playing;stateLocal=local;statePosition=Math.max(0,position);stateAt=now;
        long actions=PlaybackState.ACTION_PLAY_PAUSE|PlaybackState.ACTION_PLAY|PlaybackState.ACTION_PAUSE|PlaybackState.ACTION_SKIP_TO_NEXT|PlaybackState.ACTION_SKIP_TO_PREVIOUS|PlaybackState.ACTION_STOP;
        if(!local)actions|=PlaybackState.ACTION_SEEK_TO;
        media.setPlaybackState(new PlaybackState.Builder().setActions(actions)
            .setState(playing?PlaybackState.STATE_PLAYING:PlaybackState.STATE_PAUSED,statePosition,playing?1f:0f,now).build());
    }
    private void update(String text){
        if(spotify&&(text.startsWith("Falha")||text.startsWith("Tempo")||text.startsWith("Sessão")))scheduleRecovery(text);
        if(text.startsWith("Pronto •")&&recovering){recovering=false;control.post(resume);}
        getSystemService(NotificationManager.class).notify(1,notification(text));
        long position;boolean local;synchronized(NativeBridge.intent){position=NativeBridge.intent.position;local=NativeBridge.intent.local;}
        statePosition=-1;publishState(local?statePosition:position,local);
    }
    private long historyFrames;private final Runnable historyTick=new Runnable(){public void run(){if(destroyed)return;MusicStore store=MusicStore.get(PlaybackService.this);long frames=NativeBridge.audio.frames.get();String uri;int position;boolean local;synchronized(NativeBridge.intent){uri=NativeBridge.intent.track;position=NativeBridge.intent.position;local=NativeBridge.intent.local;}
        if(!local&&!uri.isEmpty()&&!uri.equals(store.current.optString("uri"))){org.json.JSONObject known=MusicStore.object(MusicStore.object(store.data,"tracks"),"Spotify|||"+uri);if(known.length()==0){MusicStore.put(known,"provider","Spotify");MusicStore.put(known,"id",uri);MusicStore.put(known,"uri",uri);MusicStore.put(known,"title","Faixa Spotify • "+uri.substring(Math.max(0,uri.length()-8)));MusicStore.put(known,"playable",true);}store.select(known);}
        store.setPlaybackPriority(NativeBridge.intent.desired);if(local)position=(int)(frames*1000/Math.max(1,NativeBridge.audio.sampleRate()));store.pulse(NativeBridge.playing&&frames!=historyFrames,position);historyFrames=frames;refreshMetadata();publishState(position,local);control.postDelayed(this,1000);}};
    private long resumeRevision;
    private final Runnable resume=()->{
        if(destroyed||!spotify||!NativeBridge.ready)return;
        synchronized(NativeBridge.intent){PlaybackIntent state=NativeBridge.intent;if(state.revision!=resumeRevision||!state.canRecover(SystemClock.elapsedRealtime()))return;
            if(!state.resumeSelection().isEmpty())NativeBridge.sendCommand(state.restoreCommand());}
    };
    private void cancelRecovery(){recovering=false;restartQueued=false;waitingNetwork=false;control.removeCallbacks(restart);control.removeCallbacks(resume);}
    private final Runnable restart=()->{restartQueued=false;if(!destroyed&&spotify){synchronized(NativeBridge.intent){if(!NativeBridge.intent.canRecover(SystemClock.elapsedRealtime())||NativeBridge.intent.revision!=resumeRevision){cancelRecovery();return;}}NativeBridge.sendCommand("stop");NativeBridge.audio.release();startEngine("");}};
    private final Runnable watchdog=new Runnable(){public void run(){if(destroyed)return;boolean expected; synchronized(NativeBridge.intent){expected=NativeBridge.intent.desired&&!NativeBridge.intent.local&&!NativeBridge.intent.resumeSelection().isEmpty();}
        if(spotify&&NativeBridge.ready&&expected&&online()&&SystemClock.elapsedRealtime()-Math.max(startedAt,NativeBridge.lastPcm)>30000)scheduleRecovery("Motor sem progresso PCM por 30 segundos");control.postDelayed(this,5000);}};
    private boolean online(){android.net.Network current=connectivity.getActiveNetwork();android.net.NetworkCapabilities c=current==null?null:connectivity.getNetworkCapabilities(current);return c!=null&&c.hasCapability(android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED);}
    private void scheduleRecovery(String reason){
        if(restartQueued||destroyed)return;
        synchronized(NativeBridge.intent){if(!NativeBridge.intent.beginRecovery(SystemClock.elapsedRealtime())){cancelRecovery();if(NativeBridge.intent.desired){NativeBridge.command("pause");NativeBridge.onStatus("Retomada automática expirou. Toque em Reproduzir para continuar.");}return;}resumeRevision=NativeBridge.intent.revision;}
        if(!online()){waitingNetwork=true;recovering=true;NativeBridge.onStatus("Aguardando internet para reconectar o motor…");return;}
        long now=SystemClock.elapsedRealtime();while(!restarts.isEmpty()&&now-restarts.peekFirst()>300000)restarts.removeFirst();
        if(restarts.size()>=3){Diagnostics.write("Limite de recuperação automática atingido");NativeBridge.onStatus("Recuperação interrompida após 3 tentativas. Use Conectar motor Spotify para tentar novamente.");return;}
        restarts.addLast(now);recovering=true;restartQueued=true;NativeBridge.ready=false;Diagnostics.write("Recuperação "+restarts.size()+": "+reason);NativeBridge.onStatus("Reconectando o motor Spotify…");control.postDelayed(restart,1500L*restarts.size());
    }
    private void startEngine(String token){
        startedAt=SystemClock.elapsedRealtime();NativeBridge.ready=false;
        android.content.SharedPreferences prefs=getSharedPreferences("settings",0);File cache=new File(getFilesDir(),cacheName);cache.mkdirs();NativeBridge.prepareEngineCache(cache);
        NativeBridge.start(token,cache.getAbsolutePath(),prefs.getInt("quality",320),prefs.getBoolean("normalize",true));
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){
        if(intent==null){stopSelf();return START_NOT_STICKY;}
        audioManager.requestAudioFocus(focus);
        String action=intent.getAction(),value=intent.getStringExtra("value");
        if("connect".equals(action)){
            if(BuildConfig.DEBUG)cacheName=intent.getBooleanExtra("recoveryTest",false)?"recovery-test-no-account":SpotifyClient.ENGINE_CACHE;
            sendBroadcast(new Intent("app.tonal.android.PAUSE_INDIE").setPackage(getPackageName()));
            control.removeCallbacks(restart);restartQueued=false;restarts.clear();recovering=false;waitingNetwork=false;spotify=true;NativeBridge.intent.local=false;
            NativeBridge.audio.release();NativeBridge.ready=false;
            startEngine(value==null?"":value);
        }else if("file".equals(action)){
            spotify=false;recovering=false;control.removeCallbacks(restart);restartQueued=false;NativeBridge.intent.local=true;
            sendBroadcast(new Intent("app.tonal.android.PAUSE_INDIE").setPackage(getPackageName()));
            NativeBridge.spotifyEngine=false;NativeBridge.audio.release();NativeBridge.ready=false;NativeBridge.playFile(value);
        }else if("stop".equals(action)){stopSelf();}
        return START_NOT_STICKY;
    }
    @Override public void onDestroy(){
        destroyed=true;MusicStore.get(this).setPlaybackPriority(false);MusicStore.get(this).pulse(false,NativeBridge.intent.position);MusicStore.get(this).flush();control.removeCallbacksAndMessages(null);connectivity.unregisterNetworkCallback(network);NativeBridge.commandListener=null;unregisterReceiver(switchPlayer);
        NativeBridge.command("stop");NativeBridge.audio.release();NativeBridge.ready=false;NativeBridge.playing=false;
        NativeBridge.listeners.remove(statusListener);coverWorker.shutdownNow();media.release();audioManager.abandonAudioFocusRequest(focus);
        unregisterReceiver(noisy);
        super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent){return null;}
}
