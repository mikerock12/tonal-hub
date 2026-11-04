package app.tonal.android;

import android.content.Context;
import org.json.JSONObject;

/** What a track change does to the audio already queued in the real AudioTrack. No account needed. */
final class AudioTailTests {
    private static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    private static float[] tone(int milliseconds){int frames=44100*milliseconds/1000;float[] pcm=new float[frames*2];for(int i=0;i<frames;i++)pcm[i*2]=pcm[i*2+1]=(float)(.02*Math.sin(2*Math.PI*3000*i/44100));return pcm;}
    static String run(Context context)throws Exception{
        AudioOutput out=new AudioOutput();out.initialize(context,"audio-tail-test");out.open(44100);
        try{
            for(int i=0;i<4;i++)out.write(tone(100));
            out.pause();Thread.sleep(200);
            long plainPause=out.testQueuedFrames();require(plainPause>0,"pausing keeps a tail in the track");
            out.resume();require(out.testQueuedFrames()>0,"plain resume must stay seamless");require(out.testSnapshot().getInt("staleFlushes")==0,"plain resume flushed audio");
            for(int i=0;i<4;i++)out.write(tone(100));
            out.pause();Thread.sleep(200);
            long stale=out.testQueuedFrames();require(stale>0,"a selection finds the previous track's tail");
            out.discontinuity();out.resume();
            require(out.testQueuedFrames()==0,"new track must not start with the old tail");require(out.testSnapshot().getInt("staleFlushes")==1,"tail flush not counted");
            for(int i=0;i<2;i++)out.write(tone(100));require(out.isPlaying(),"new audio plays after the flush");
            out.discontinuity();out.pause();Thread.sleep(120);out.resume();require(out.testSnapshot().getInt("staleFlushes")==1,"a mark made while playing cannot flush a later pause");
            return new JSONObject().put("passed",true).put("plainPauseQueuedMs",plainPause*1000/44100).put("staleTailMs",stale*1000/44100).put("queuedAfterFlush",0).put("realAudioTrack",true).put("liveSpotify",false).put("bluetooth",false).toString();
        }finally{out.release();}
    }
}
