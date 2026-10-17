package app.tonal.android;

import android.media.*;
import java.io.*;
import java.nio.*;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Consumer;

/** Platform compressed-audio decoder -> the same float PCM/DSP/AudioTrack used by Tonalify. */
final class IndieDecoder implements AutoCloseable {
    private final AtomicLong generation=new AtomicLong();
    private volatile ProgressiveSource source;private volatile boolean paused,closed;
    private volatile long position;private volatile Thread worker;
    interface Observer {void status(String text);void ended();}
    private final Observer observer;private final File cache;
    IndieDecoder(File cache,Observer observer){this.cache=cache;this.observer=observer;}
    void play(String url){closeTrack();closed=false;paused=false;position=0;long epoch=generation.incrementAndGet();worker=new Thread(()->decode(epoch,url),"indie-decoder");worker.start();}
    void pause(){paused=true;ProgressiveSource s=source;if(s!=null)s.pausePrefetch(true);NativeBridge.onAudioPause();}
    void resume(){paused=false;ProgressiveSource s=source;if(s!=null)s.pausePrefetch(false);NativeBridge.onAudioResume();}
    boolean playing(){return !paused&&worker!=null&&worker.isAlive();}
    long position(){return position;}
    long bufferedBytes(){ProgressiveSource s=source;return s==null?0:s.bufferedBytes();}
    private void decode(long epoch,String url){MediaExtractor extractor=new MediaExtractor();MediaCodec codec=null;ProgressiveSource own=null;
        try{
            observer.status("Preenchendo a reserva de áudio…");own=new ProgressiveSource(cache,url,text->{if(epoch==generation.get())observer.status(text);});if(epoch!=generation.get()){own.close();return;}source=own;own.pausePrefetch(paused);extractor.setDataSource(own);
            int audio=-1;MediaFormat format=null;for(int n=0;n<extractor.getTrackCount();n++){MediaFormat f=extractor.getTrackFormat(n);String mime=f.getString(MediaFormat.KEY_MIME);if(mime!=null&&mime.startsWith("audio/")){audio=n;format=f;break;}}
            if(audio<0||format==null)throw new IOException("Nenhuma faixa de áudio compatível");extractor.selectTrack(audio);
            codec=MediaCodec.createDecoderByType(format.getString(MediaFormat.KEY_MIME));codec.configure(format,null,null,0);codec.start();boolean inputEnd=false,outputEnd=false,opened=false;int encoding=AudioFormat.ENCODING_PCM_16BIT,channels=2;MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
            while(epoch==generation.get()&&!closed&&!outputEnd){
                if(paused){Thread.sleep(10);continue;}
                if(!inputEnd){int index=codec.dequeueInputBuffer(10000);if(index>=0){ByteBuffer input=codec.getInputBuffer(index);int size=extractor.readSampleData(input,0);if(size<0){codec.queueInputBuffer(index,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM);inputEnd=true;}else{codec.queueInputBuffer(index,0,size,extractor.getSampleTime(),0);extractor.advance();}}}
                int index=codec.dequeueOutputBuffer(info,10000);
                if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat f=codec.getOutputFormat();channels=f.getInteger(MediaFormat.KEY_CHANNEL_COUNT);int rate=f.getInteger(MediaFormat.KEY_SAMPLE_RATE);encoding=f.containsKey(MediaFormat.KEY_PCM_ENCODING)?f.getInteger(MediaFormat.KEY_PCM_ENCODING):AudioFormat.ENCODING_PCM_16BIT;if(encoding!=AudioFormat.ENCODING_PCM_16BIT&&encoding!=AudioFormat.ENCODING_PCM_FLOAT)throw new IOException("Formato PCM não suportado");if(epoch!=generation.get())break;NativeBridge.audio.source(false,false,-70,0,Double.NaN,0);NativeBridge.onAudioFormat(rate,channels);opened=true;observer.status("Reproduzindo • reserva progressiva");}
                else if(index>=0){try{if(info.size>0&&opened&&epoch==generation.get()){ByteBuffer out=codec.getOutputBuffer(index).order(ByteOrder.nativeOrder());out.position(info.offset);out.limit(info.offset+info.size);int count=info.size/(encoding==AudioFormat.ENCODING_PCM_FLOAT?4:2);float[] samples=new float[count];if(encoding==AudioFormat.ENCODING_PCM_FLOAT)out.asFloatBuffer().get(samples);else{ShortBuffer shorts=out.asShortBuffer();for(int n=0;n<count;n++)samples[n]=shorts.get()/32768f;}NativeBridge.onPcm(samples);position=info.presentationTimeUs/1000;}outputEnd=(info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0;}finally{codec.releaseOutputBuffer(index,false);}}
            }
            if(epoch==generation.get()&&!closed){NativeBridge.onAudioEnd();observer.ended();}
        }catch(Exception error){if(epoch==generation.get()&&!closed){NativeBridge.onAudioPause();Diagnostics.write("Indie decoder: "+error.getClass().getSimpleName());observer.status("Não foi possível reproduzir. Verifique a conexão e tente novamente.");}}
        finally{if(codec!=null){try{codec.stop();}catch(Exception ignored){}codec.release();}extractor.release();if(own!=null)own.close();if(source==own)source=null;}
    }
    private void closeTrack(){generation.incrementAndGet();ProgressiveSource s=source;if(s!=null)s.close();Thread t=worker;if(t!=null)t.interrupt();NativeBridge.audio.release();}
    public void close(){closed=true;closeTrack();}
}
