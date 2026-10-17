package app.tonal.android;

/** Serialize PCM processing with reset/close so callbacks cannot use a freed JNI handle. */
final class Dsp implements AutoCloseable {
    static final int[] FREQUENCIES={31,62,125,250,500,1000,2000,4000,8000,16000};
    static final class Settings {
        final float[] gains,values;final float bass;final boolean compressor;
        Settings(float[] gains,float bass,boolean compressor){this(true,true,gains,bass,compressor,-1,0,false);}
        Settings(boolean enabled,boolean eq,float[] gains,float bass,boolean compressor,float ceiling,float gain,boolean hrtf){
            this.gains=gains.clone();this.bass=bass;this.compressor=compressor;values=new float[17];
            values[0]=enabled?1:0;values[1]=eq?1:0;System.arraycopy(gains,0,values,2,10);values[12]=bass;values[13]=compressor?1:0;values[14]=ceiling;values[15]=gain;values[16]=hrtf?1:0;
        }
        boolean active(){if(values[0]==0)return false;if(bass!=0||compressor||values[15]!=0||values[16]!=0)return true;if(values[1]!=0)for(float gain:gains)if(gain!=0)return true;return false;}
    }
    volatile Settings settings=new Settings(new float[10],0,false);
    private Settings applied;private long handle;private int rate=44100,channels=2,settling;private boolean wet,binaural;
    synchronized void reset(int rate){reset(rate,2,false);}
    synchronized void reset(int rate,int channels,boolean binaural){close();this.rate=rate;this.channels=channels;this.binaural=binaural;wet=false;settling=0;handle=NativeAudio.create(rate,channels,binaural);applied=null;}
    synchronized float[] render(float[] samples){
        if(samples.length==0)return samples;
        if(handle==0)reset(rate,channels,binaural);Settings next=settings;boolean active=next.active();
        if(!active&&!wet&&channels==2){
            // Flat stereo needs no JNI copies, filter graph or HRTF work.
            for(float x:samples)if(!Float.isFinite(x)){float[] clean=samples.clone();for(int i=0;i<clean.length;i++)if(!Float.isFinite(clean[i]))clean[i]=0;return clean;}
            return samples;
        }
        try{
            if(next!=applied){NativeAudio.configure(handle,next.values);applied=next;if(!active)settling=rate*30/1000;}
            float[] result=NativeAudio.process(handle,samples);if(active)wet=true;else if((settling-=samples.length/channels)<=0)wet=false;return result;
        }catch(IllegalStateException error){close();throw error;}
    }
    synchronized void process(float[] samples){float[] result=render(samples);if(result.length!=samples.length)throw new IllegalStateException("Use render para áudio multicanal");System.arraycopy(result,0,samples,0,samples.length);}
    public synchronized void close(){if(handle!=0)NativeAudio.destroy(handle);handle=0;applied=null;}
    static void selfTest(){AudioSelfTest.dsp();}
}
