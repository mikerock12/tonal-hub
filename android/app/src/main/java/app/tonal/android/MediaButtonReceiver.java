package app.tonal.android;

import android.content.*;

/**
 * Registered on the Tonalify and IndieTonal media sessions so that Android remembers this app,
 * not the Spotify app, as the last media-button target. While a player service runs, its session
 * receives the keys directly. Once it has stopped, a key from a headset or car arrives here and is
 * ignored: it neither resumes the music nor wakes another player in the background.
 */
public final class MediaButtonReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context,Intent intent){
        if(!Intent.ACTION_MEDIA_BUTTON.equals(intent.getAction()))return;
        android.view.KeyEvent key=intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
        if(key!=null&&key.getAction()!=android.view.KeyEvent.ACTION_DOWN)return;
        Diagnostics.initializeIfAbsent(context,"tonalify");Diagnostics.write("Botão de mídia com o player fechado ignorado");
    }
    static void attach(android.media.session.MediaSession media,Context context){
        if(android.os.Build.VERSION.SDK_INT>=33)media.setMediaButtonBroadcastReceiver(new ComponentName(context,MediaButtonReceiver.class));
        else media.setMediaButtonReceiver(android.app.PendingIntent.getBroadcast(context,7,new Intent(Intent.ACTION_MEDIA_BUTTON).setClass(context,MediaButtonReceiver.class),android.app.PendingIntent.FLAG_IMMUTABLE));
    }
}
