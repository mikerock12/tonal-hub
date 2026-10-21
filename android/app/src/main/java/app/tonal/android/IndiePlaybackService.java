package app.tonal.android;

import android.app.*;
import android.content.*;
import android.media.*;
import android.media.session.*;
import android.os.*;
import java.util.*;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;

/** Own process, media session, audio focus and credentials. Never controls remote Spotify devices. */
public final class IndiePlaybackService extends Service {
    static final CopyOnWriteArrayList<Consumer<String>> listeners=new CopyOnWriteArrayList<>();
    static volatile String status="Escolha uma faixa pública para ouvir",title="IndieTonal",artist="";
    static volatile boolean playing;static volatile long position,buffered;
    private static ArrayList<IndieCatalog.Item> selection=new ArrayList<>();private static int selected;
    private final Handler main=new Handler(Looper.getMainLooper());private IndieDecoder decoder;private IndieCatalog catalog;
    private MediaSession media;private AudioManager manager;private AudioFocusRequest focus;private boolean destroyed,wanted;
    private final BroadcastReceiver pauseReceiver=new BroadcastReceiver(){public void onReceive(Context c,Intent i){pause();}};
    static void play(Context context,List<IndieCatalog.Item> items,int index){selection=new ArrayList<>(items);selected=index;start(context,"selection");}
    static void start(Context context,String action){context.startForegroundService(new Intent(context,IndiePlaybackService.class).setAction(action));}
    @Override public void onCreate(){
        super.onCreate();AppTheme.apply(this);Diagnostics.initialize(this,"indie");NativeBridge.audio.initialize(this,"indie-audio");catalog=new IndieCatalog(this);
        decoder=new IndieDecoder(getCacheDir(),new IndieDecoder.Observer(){public void status(String text){main.post(()->{if(!destroyed){playing=wanted&&decoder.playing();update(text);}});}public void ended(){main.post(()->{if(!destroyed&&wanted){playing=false;if(selected+1<selection.size()){selected++;open();}else {wanted=false;update("Fila finalizada");}}});}});
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel("indie-playback","IndieTonal • reprodução",NotificationManager.IMPORTANCE_LOW));
        media=new MediaSession(this,"IndieTonal");media.setCallback(new MediaSession.Callback(){public void onPlay(){if(!wanted&&getSystemService(KeyguardManager.class).isKeyguardLocked()&&!getSharedPreferences("settings",0).getBoolean("allowLockedMediaResume",false))return;resume();}public void onPause(){pause();}public void onSkipToNext(){if(wanted)next(1);}public void onSkipToPrevious(){if(wanted)next(-1);}public void onStop(){stopSelf();}});media.setActive(true);
        manager=getSystemService(AudioManager.class);focus=new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setOnAudioFocusChangeListener(change->{if(change<0)pause();}).build();
        IntentFilter filter=new IntentFilter("app.tonal.android.PAUSE_INDIE");filter.addAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);if(Build.VERSION.SDK_INT>=33)registerReceiver(pauseReceiver,filter,Context.RECEIVER_NOT_EXPORTED);else registerReceiver(pauseReceiver,filter);
        startForeground(2,notification());main.post(tick);
    }
    private Notification notification(){PendingIntent open=PendingIntent.getActivity(this,2,new Intent(this,IndieActivity.class),PendingIntent.FLAG_IMMUTABLE);return new Notification.Builder(this,"indie-playback").setSmallIcon(R.drawable.ic_tonal).setColor(Ui.BLUE).setLargeIcon(cover).setContentTitle(title).setContentText(artist+" • "+status).setContentIntent(open).setStyle(new Notification.MediaStyle().setMediaSession(media.getSessionToken())).setOngoing(playing).build();}
    // Cover for the notification and lock screen, loaded off the main thread.
    private final java.util.concurrent.ExecutorService coverWorker=java.util.concurrent.Executors.newSingleThreadExecutor();
    private android.graphics.Bitmap cover;private String coverUrl="";
    private void showItem(IndieCatalog.Item item){if(!item.image.equals(coverUrl)){coverUrl=item.image;cover=null;String wanted=item.image;if(!wanted.isEmpty())coverWorker.execute(()->{android.graphics.Bitmap art=Artwork.fetch(this,wanted);if(art!=null)main.post(()->{if(!destroyed&&wanted.equals(coverUrl)){cover=art;showItem(item);}});});}
        MediaMetadata.Builder metadata=new MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,title).putString(MediaMetadata.METADATA_KEY_ARTIST,artist).putString(MediaMetadata.METADATA_KEY_ALBUM,item.album).putLong(MediaMetadata.METADATA_KEY_DURATION,item.duration);
        if(cover!=null)metadata.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART,cover);media.setMetadata(metadata.build());getSystemService(NotificationManager.class).notify(2,notification());}
    private void update(String text){status=text;Diagnostics.write(text);getSystemService(NotificationManager.class).notify(2,notification());media.setPlaybackState(new PlaybackState.Builder().setActions(PlaybackState.ACTION_PLAY|PlaybackState.ACTION_PAUSE|PlaybackState.ACTION_SKIP_TO_NEXT|PlaybackState.ACTION_SKIP_TO_PREVIOUS|PlaybackState.ACTION_STOP).setState(playing?PlaybackState.STATE_PLAYING:PlaybackState.STATE_PAUSED,position,playing?1:0).build());for(Consumer<String> listener:listeners)listener.accept(text);}
    private final Runnable tick=new Runnable(){public void run(){if(destroyed)return;position=decoder.position();buffered=decoder.bufferedBytes();MusicStore.get(IndiePlaybackService.this).pulse(playing,(int)position);main.postDelayed(this,1000);}};
    private void open(){if(selected<0||selected>=selection.size())return;IndieCatalog.Item item=selection.get(selected);if(!item.playable)return;sendBroadcast(new Intent("app.tonal.android.PAUSE_TONALIFY").setPackage(getPackageName()));if(manager.requestAudioFocus(focus)!=AudioManager.AUDIOFOCUS_REQUEST_GRANTED){update("Saída de áudio ocupada. Toque em Reproduzir para tentar novamente.");return;}MusicStore.get(this).select(item.saved());title=item.title;artist=item.artist;showItem(item);
        try{String url=catalog.stream(item);playing=true;update("Preparando áudio…");decoder.play(url);}catch(Exception error){playing=false;update("Fonte indisponível ou conexão alterada. Refaça a busca.");}}
    private void pause(){wanted=false;MusicStore.get(this).pulse(false,(int)position);decoder.pause();playing=false;update("Pausado");}
    private void resume(){if(selection.isEmpty()||manager.requestAudioFocus(focus)!=AudioManager.AUDIOFOCUS_REQUEST_GRANTED)return;wanted=true;sendBroadcast(new Intent("app.tonal.android.PAUSE_TONALIFY").setPackage(getPackageName()));if(!decoder.playing()&&decoder.position()==0){open();return;}decoder.resume();playing=true;update("Reproduzindo");}
    private void next(int step){int index=selected+step;if(index>=0&&index<selection.size()){selected=index;open();}}
    @Override public int onStartCommand(Intent intent,int flags,int id){if(intent==null){stopSelf();return START_NOT_STICKY;}String action=intent.getAction();if("selection".equals(action)){wanted=true;open();}else if("pause".equals(action))pause();else if("play".equals(action))resume();else if("toggle".equals(action)){if(playing)pause();else resume();}else if("next".equals(action)){wanted=true;next(1);}else if("prev".equals(action)){wanted=true;next(-1);}else if("stop".equals(action))stopSelf();return START_NOT_STICKY;}
    @Override public void onDestroy(){destroyed=true;MusicStore.get(this).pulse(false,(int)position);MusicStore.get(this).flush();main.removeCallbacksAndMessages(null);decoder.close();catalog.close();playing=false;manager.abandonAudioFocusRequest(focus);media.release();unregisterReceiver(pauseReceiver);status="Motor parado";for(Consumer<String> l:listeners)l.accept(status);super.onDestroy();}
    @Override public IBinder onBind(Intent intent){return null;}
}
