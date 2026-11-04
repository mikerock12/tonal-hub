package app.tonal.android;

import android.app.ActivityManager;
import android.content.Context;
import android.os.PowerManager;
import android.os.SystemClock;

/** Blink patterns (mirrored in ChristmasGarland.qml) and the guard that puts music first. */
final class ChristmasLights {
    private static final long SETTLE_MS=8000,WINDOW_MS=20_000,QUIET_MS=15_000;private static final int FAILURES=3;
    private static long nextCheck,quietUntil,windowStart;private static int underruns=-1,failures;
    private static boolean saving;
    static final int TICK_MS=67;static final float STEP=TICK_MS/1000f;
    private static final android.os.Handler main=new android.os.Handler(android.os.Looper.getMainLooper());
    private static final java.util.ArrayList<Runnable> clients=new java.util.ArrayList<>();
    private static boolean ticking;
    private static final Runnable clock=new Runnable(){public void run(){
        for(Runnable client:clients.toArray(new Runnable[0]))client.run();
        if(clients.isEmpty())ticking=false;else main.postDelayed(this,TICK_MS);
    }};
    private ChristmasLights(){}
    /** Everything joins one 15 fps clock, so their invalidations coalesce into a single frame. */
    static void join(Runnable client){if(!clients.contains(client))clients.add(client);if(!ticking){ticking=true;main.postDelayed(clock,TICK_MS);}}
    static void leave(Runnable client){clients.remove(client);}

    static float hash(int i){double value=Math.abs(Math.sin(i*12.9898)*43758.5453);return (float)(value-Math.floor(value));}
    private static float pattern(int kind,int i,float h,float t){
        switch(kind){
            case 0:return .5f+.5f*(float)Math.sin(t*(1.4f+2.6f*h)+h*6.283f);
            case 1:return (float)Math.pow(Math.max(0,Math.cos(i*.55f-t*3.2f)),3);
            case 2:return .5f+.5f*(float)Math.sin(t*2.4f+(i%2)*Math.PI);
            default:return (float)Math.pow(.5f+.5f*Math.sin(t*(5+7*h)+h*40),6);
        }
    }
    /** Brightness 0..1 of bulb i. Four patterns take turns, cross-fading into the next one. */
    static float level(int i,float t){
        float h=hash(i),within=(t%7)/7,mix=Math.min(1,Math.max(0,(within-.86f)/.14f));int slot=(int)Math.floor(t/7)%4;
        return .16f+.84f*(pattern(slot,i,h,t)*(1-mix)+pattern((slot+1)%4,i,h,t)*mix);
    }
    static float twinkle(int i,float t){float h=hash(i);return .58f+.42f*(.5f+.5f*(float)Math.sin(t*(1.8f+3*h)+h*6.283f));}

    /**
     * Decorations yield to audio, but only when they could be the cause: repeated output
     * underruns during steady playback silence them for 15 s. A track start is excluded,
     * because the next song arriving late from the network runs the output dry whatever
     * is on screen, and a single gap proves nothing. Battery saver and low-RAM devices
     * also stop them. Polled sparsely, never from the PCM path.
     */
    static boolean allowed(Context context){
        long now=SystemClock.elapsedRealtime();
        if(now>=nextCheck){
            nextCheck=now+2000;
            PowerManager power=context.getSystemService(PowerManager.class);ActivityManager memory=context.getSystemService(ActivityManager.class);
            saving=(power!=null&&power.isPowerSaveMode())||(memory!=null&&memory.isLowRamDevice());
            int current=NativeBridge.audio.underruns();
            if(underruns>=0&&current>underruns&&now-NativeBridge.audio.lastTransition()>=SETTLE_MS){
                if(now-windowStart>WINDOW_MS){windowStart=now;failures=0;}
                failures+=current-underruns;
                if(failures>=FAILURES){failures=0;quietUntil=now+QUIET_MS;Diagnostics.write("Natal: animações em pausa para preservar o áudio");}
            }
            underruns=current;
        }
        return !saving&&now>=quietUntil;
    }
}
