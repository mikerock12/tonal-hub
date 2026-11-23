package app.tonal.android;

import android.content.Context;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import org.json.*;

/** Signal-level tests exercise the shipped native libraries, not a second model of the DSP. */
final class AudioSelfTest {
    static void require(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static void until(java.util.function.BooleanSupplier condition,String message)throws InterruptedException{long deadline=android.os.SystemClock.elapsedRealtime()+10000;while(!condition.getAsBoolean()&&android.os.SystemClock.elapsedRealtime()<deadline)Thread.sleep(5);require(condition.getAsBoolean(),message);}
    static float[] tone(int channels,int channel,double amplitude,double hz){float[] a=new float[48000*channels];for(int f=0;f<48000;f++)a[f*channels+channel]=(float)(amplitude*Math.sin(2*Math.PI*hz*f/48000));return a;}
    static double rms(float[] a,int channel,int channels){double e=0;int count=0;for(int i=24000*channels+channel;i<a.length;i+=channels){e+=a[i]*a[i];count++;}return Math.sqrt(e/count);}
    static Dsp.Settings setting(boolean enabled,boolean eq,float[] bands,float bass,boolean compressor,float ceiling,float gain,boolean hrtf){return new Dsp.Settings(enabled,eq,bands,bass,compressor,ceiling,gain,hrtf);}
    static void dsp(){
        try(Dsp d=new Dsp()){
            float[] stereo=tone(2,0,.05,1000);d.reset(48000);require(Arrays.equals(stereo,d.render(stereo)),"Bypass alterou PCM");
            float[] bands=new float[10];bands[5]=3;d.settings=new Dsp.Settings(bands,0,false);float[] eq=d.render(stereo);double ratio=rms(eq,0,2)/rms(stereo,0,2);require(ratio>1.35&&ratio<1.48,"EQ 1 kHz +3 dB: "+ratio);
            for(int band=0;band<10;band++){d.reset(48000);float[] curve=new float[10];curve[band]=3;d.settings=setting(true,true,curve,0,false,-1,0,false);float[] input=tone(2,0,.025,Dsp.FREQUENCIES[band]);double measured=rms(d.render(input),0,2)/rms(input,0,2);require(measured>1.34&&measured<1.49,"EQ band "+band+": "+measured);}
            AudioConfig profile=new AudioConfig();profile.profile=2;profile.spatial=1;require(profile.snapshot(0,false).values[16]==0,"headphone HRTF blocked on car profile");profile.spatial=2;require(profile.snapshot(0,false).values[16]==1,"explicit speaker HRTF enabled");profile.enabled=false;require(!profile.snapshot(0,false).active(),"master bypass dominates spatial request");
            d.reset(48000);d.settings=setting(true,false,new float[10],6,false,-1,0,false);float[] low=tone(2,0,.03,30);float[] bass=d.render(low);ratio=rms(bass,0,2)/rms(low,0,2);require(ratio>1.8&&ratio<2.1,"Shelf de graves: "+ratio);
            d.reset(48000);d.settings=setting(true,false,new float[10],0,true,-1,0,false);float[] loud=tone(2,0,.6,1000);float[] comp=d.render(loud);ratio=rms(comp,0,2)/rms(loud,0,2);require(ratio>.3&&ratio<.8,"Compressor: "+ratio);
            d.reset(48000);Arrays.fill(bands,12);d.settings=setting(true,true,bands,12,true,-2,12,false);float[] limit=d.render(loud);double ceiling=Math.pow(10,-2./20);for(int i=4800;i<limit.length;i++)require(Float.isFinite(limit[i])&&Math.abs(limit[i])<=ceiling+.0001,"Teto/PCM inválido");
            d.reset(48000);d.settings=setting(false,true,bands,12,true,-2,12,true);require(Arrays.equals(stereo,d.render(stereo)),"Bypass mestre alterou PCM");
            d.reset(48000);d.settings=setting(true,false,new float[10],0,false,-1,6,false);float[] constant=new float[96000];Arrays.fill(constant,.05f);float[] ramp=d.render(constant);double jump=0;for(int i=2;i<ramp.length;i+=2)jump=Math.max(jump,Math.abs(ramp[i]-ramp[i-2]));require(jump<.02,"Transição DSP abrupta: "+jump);
            d.reset(48000);d.settings=setting(true,false,new float[10],0,false,-1,0,true);float[] hrtf=d.render(stereo);require(rms(hrtf,1,2)>.0001&&!Arrays.equals(hrtf,stereo),"HRTF sem resposta no outro ouvido");
            d.reset(48000);float[] chunked=new float[stereo.length];
            for(int offset=0;offset<stereo.length;offset+=514){float[] block=d.render(Arrays.copyOfRange(stereo,offset,Math.min(stereo.length,offset+514)));System.arraycopy(block,0,chunked,offset,block.length);}
            double difference=0;int compared=0;for(int i=9600;i<hrtf.length;i++){double delta=hrtf[i]-chunked[i];difference+=delta*delta;compared++;}
            require(Math.sqrt(difference/compared)<.0005,"Descontinuidade HRTF entre blocos: "+Math.sqrt(difference/compared));
            d.reset(48000,2,true);require(Arrays.equals(stereo,d.render(stereo)),"IAMF recebeu segunda HRTF");
            for(int channels:new int[]{1,4,6,8}){d.reset(48000,channels,false);float[] surround=d.render(tone(channels,channels==6||channels==8?2:0,.05,700));require(surround.length==96000&&rms(surround,0,2)>.00001,"Render multicanal "+channels);}
            d.reset(48000);d.settings=new Dsp.Settings(new float[10],0,false);float[] bad={Float.NaN,Float.POSITIVE_INFINITY,.2f,-.2f};float[] clean=d.render(bad);for(float x:clean)require(Float.isFinite(x),"PCM não finito");
        }
        require(AudioConfig.bufferMillis(1,true)>AudioConfig.bufferMillis(0,true)&&AudioConfig.bufferMillis(0,true)>AudioConfig.bufferMillis(2,true),"Perfis de buffer");
        require(AudioConfig.growBuffer(4000,48000,4096)==4096&&AudioConfig.growBuffer(12000,48000,24000)==12000,"Limites do buffer adaptativo");
    }
    /**
     * Mixer de transições na saída real: marcador de começo de faixa, leitura adiantada sem bloquear,
     * fim de A pelo marcador apesar de uma estimativa errada e drenagem sem a próxima faixa.
     */
    static JSONObject transitions(Context context)throws Exception{
        JSONObject report=new JSONObject();AudioOutput out=new AudioOutput();out.initialize(context,"transition-test");
        try{
            out.startStereo(44100);
            float[] a=new float[44100*2*12],b=new float[44100*2*12];
            for(int f=0;f<44100*12;f++){float v=(float)(.4*Math.sin(2*Math.PI*440*f/44100.));a[f*2]=v;a[f*2+1]=v;}
            for(int f=0;f<44100*12;f++){float v=(float)(.4*Math.sin(2*Math.PI*660*f/44100.));b[f*2]=v;b[f*2+1]=v;}
            int packet=4096,head=44100*2*6;
            // A toca direto até faltarem 6 s; o librespot marca o começo de cada faixa.
            out.boundary(0);
            for(int i=0;i<head;i+=packet)out.write(Arrays.copyOfRange(a,i,Math.min(head,i+packet)));
            long counted=out.trackPositionMs();
            require(Math.abs(counted-6000)<50,"posição exata desde o marcador: "+counted+" ms");
            int before=out.underruns(),boundaries=out.markerBoundaries;
            // Estimativa 2,5 s curta de propósito (o erro da 0.17.1): o fim de A vem do marcador.
            require(out.armTransition(0,3500,3000,4000)!=0,"armar a primeira transição");
            long start=System.nanoTime();
            for(int i=head;i<a.length;i+=packet)out.write(Arrays.copyOfRange(a,i,Math.min(a.length,i+packet)));
            long captureMs=(System.nanoTime()-start)/1_000_000;
            require(captureMs<1000,"leitura adiantada bloqueou: "+captureMs+" ms para 6 s de áudio");
            // O motor demora para começar B: a saída não pode parar nem a mescla começar antes.
            Thread.sleep(1000);
            require(out.transitionState()==1,"a mescla começou antes do ponto: "+out.transitionState());
            out.boundary(0);
            until(()->out.markerBoundaries==boundaries+1,"o marcador não atravessou a fila na ordem");
            require(out.markerBoundaries==boundaries+1,"o fim de A não veio do marcador");
            start=System.nanoTime();
            for(int i=0;i<b.length;i+=packet)out.write(Arrays.copyOfRange(b,i,Math.min(b.length,i+packet)));
            require(Math.abs(out.trackPositionMs()-12000)<50,"posição de B recomeça no marcador: "+out.trackPositionMs());
            until(()->out.transitionState()==3,"a primeira mescla não concluiu pela fila SPSC");
            long bMs=(System.nanoTime()-start)/1_000_000;
            int latency=out.transitionLatencyMs();
            require(out.transitionState()==3&&latency>=2000&&latency<=3200,"pré-rolagem depois da mescla: "+out.transitionState()+" "+latency+" ms");
            long[] trace=out.transitionTrace();
            require(trace!=null&&trace[TransitionNative.T_PHASE]==TransitionNative.PHASE_COMPLETED&&TransitionNative.overlapVerified(trace)
                    &&trace[TransitionNative.T_MISSING_B]==0&&trace[TransitionNative.T_PREPARED_B]>=44100,
                "a mescla não provou A e B juntas: "+java.util.Arrays.toString(trace));
            report.put("captureMs",captureMs).put("bWriteMs",bMs).put("latencyAfterMixMs",latency)
                .put("preparedBMs",trace[TransitionNative.T_PREPARED_B]*1000/44100).put("bFramesInMix",trace[TransitionNative.T_B_FRAMES]);
            // Segunda transição com o atraso da primeira no buffer: o fim de B já está guardado.
            require(out.armTransition(2,0,2000,2000)!=0,"armar com o atraso da anterior");
            out.boundary(0);  // o fim de B é o que já está no buffer
            start=System.nanoTime();
            for(int i=0;i<a.length;i+=packet)out.write(Arrays.copyOfRange(a,i,Math.min(a.length,i+packet)));
            until(()->out.transitionState()==3,"a segunda mescla não concluiu pela fila SPSC");
            int latency2=out.transitionLatencyMs();
            require(out.transitionState()==3&&latency2>=2000&&latency2<=3200,"segunda transição: "+out.transitionState()+" "+latency2+" ms");
            report.put("secondWriteMs",(System.nanoTime()-start)/1_000_000).put("latencyAfterSecondMs",latency2);
            // Sem a próxima faixa: o motor fecha o sink e o fim lido toca sozinho.
            out.cancelTransition();
            require(out.armTransition(4,4000,2000,3000)!=0,"armar transição sem próxima");
            float[] tail=Arrays.copyOfRange(b,0,44100*2*3);
            for(int i=0;i<tail.length;i+=packet)out.write(Arrays.copyOfRange(tail,i,Math.min(tail.length,i+packet)));
            require(out.continuesThroughEngineStop(),"fim natural recusado");
            long until=android.os.SystemClock.elapsedRealtime()+8000;
            while(out.transitionState()!=0&&android.os.SystemClock.elapsedRealtime()<until)Thread.sleep(50);
            require(out.transitionState()==0,"o fim lido não terminou");
            // Recuperação de buffering no meio (motor pausa e retoma A no mesmo ponto):
            // o trecho retomado continua sendo A; a mescla usa a B de verdade.
            long sequence=out.trackSequence;
            require(out.armTransition(0,6000,2000,2000)!=0,"armar antes da recuperação");
            for(int i=0;i<44100*2*3;i+=packet)out.write(Arrays.copyOfRange(a,i,Math.min(44100*2*3,i+packet)));
            require(out.continuesThroughEngineStop(),"pausa de recuperação recusada");
            out.boundary(3000);
            require(out.trackSequence==sequence,"retomada no meio virou outra ocorrência");
            for(int i=44100*2*3;i<44100*2*6;i+=packet)out.write(Arrays.copyOfRange(a,i,Math.min(44100*2*6,i+packet)));
            out.boundary(0);
            require(out.trackSequence==sequence+1,"a próxima faixa não virou uma nova ocorrência");
            for(int i=0;i<44100*2*5;i+=packet)out.write(Arrays.copyOfRange(b,i,Math.min(44100*2*5,i+packet)));
            until=android.os.SystemClock.elapsedRealtime()+10000;
            while(out.transitionState()!=3&&out.transitionState()!=0&&android.os.SystemClock.elapsedRealtime()<until)Thread.sleep(50);
            long[] resumed=out.transitionTrace();
            require(resumed!=null&&resumed[TransitionNative.T_PHASE]==TransitionNative.PHASE_COMPLETED&&TransitionNative.overlapVerified(resumed)
                    &&resumed[TransitionNative.T_BOUNDARY]==44100L*6,"recuperação no meio: "+java.util.Arrays.toString(resumed));
            report.put("recoveryResumeVerified",true);
            out.cancelTransition();
            // Cancelar descarta o que foi lido e volta ao caminho direto.
            require(out.armTransition(5,4000,2000,2000)!=0,"armar para cancelar");out.write(Arrays.copyOfRange(b,0,packet*8));out.cancelTransition();
            require(out.transitionState()==0&&out.transitionLatencyMs()==0,"cancelar manteve a transição");out.write(Arrays.copyOfRange(b,0,packet));
            out.prepareTransportChange();long blockedFrames=out.frames.get();out.write(Arrays.copyOfRange(a,0,packet));require(out.frames.get()==blockedFrames,"PCM antigo atravessou a troca de transporte");out.completeTransportChange();out.write(Arrays.copyOfRange(a,0,packet));require(out.frames.get()>blockedFrames,"PCM novo não retomou depois do evento tipado");
            JSONObject output=out.testSnapshot();report.put("underruns",out.underruns()-before).put("queueWaits",output.getLong("transitionQueueWaits")).put("queueMaxSize",output.getInt("transitionQueueMaxSize")).put("queueWaitMs",output.getLong("transitionQueueWaitMs")).put("directFallbacks",output.getLong("transitionDirectFallbacks")).put("pass",true);
        }finally{out.release();}
        return report;
    }
    static void run(Context context){
        JSONObject report=new JSONObject();JSONArray checks=new JSONArray();
        try{
            dsp();checks.put("native_dsp_bypass_eq_bass_compressor_limiter_ramp_hrtf_multichannel_binaural_nan");
            File fixtures=new File(context.getFilesDir(),"audio-fixtures");require(fixtures.isDirectory(),"Fixtures de teste ausentes");File dir=new File(context.getCacheDir(),"audio-suite");dir.mkdirs();
            for(String name:new String[]{"stereo-44100.wav","stereo-96000.wav","stereo.flac","stereo.mp3","stereo.m4a","replaygain.flac","surround-51.wav","surround-71.wav","stereo.iamf","surround.iamf","quiet.wav","loud.wav"}){
                long generation=AudioFiles.cancel();File input=new File(fixtures,name),out=new File(dir,"prepared.wav");
                JSONObject info=new JSONObject(NativeAudio.prepare(input.getAbsolutePath(),out.getAbsolutePath(),name.endsWith(".iamf"),generation));
                require(out.length()>68&&info.getInt("rate")>0&&Double.isFinite(info.getDouble("lufs"))&&info.getDouble("peak")>0,"Preparação "+name);
                if(name.startsWith("stereo"))require(info.getDouble("lufs")>-40&&info.getDouble("lufs")<-10,"LUFS da fixture "+name+": "+info.getDouble("lufs"));
                require(info.getBoolean("binaural")==name.endsWith(".iamf"),"Estado binaural "+name);
                if(name.equals("replaygain.flac"))require(!info.isNull("replayGain"),"ReplayGain não lido");
                if(name.contains("51"))require(info.getInt("channels")==6,"5.1 descartado");if(name.contains("71"))require(info.getInt("channels")==8,"7.1 descartado");
                checks.put(new JSONObject().put("file",name).put("metadata",info));out.delete();
            }
            boolean rejected=false;try{long generation=AudioFiles.cancel();NativeAudio.prepare(new File(fixtures,"corrupt.bin").getAbsolutePath(),new File(dir,"bad.wav").getAbsolutePath(),false,generation);}catch(IllegalStateException expected){rejected=true;}require(rejected,"Arquivo inválido aceito");checks.put("invalid_file_rejected");
            long generation=AudioFiles.cancel();NativeAudio.cancel(generation+1);rejected=false;try{NativeAudio.prepare(new File(fixtures,"stereo.flac").getAbsolutePath(),new File(dir,"cancel.wav").getAbsolutePath(),false,generation);}catch(IllegalStateException expected){rejected=true;}require(rejected,"Cancelamento ignorado");checks.put("decode_cancellation");
            android.content.SharedPreferences prefs=context.getSharedPreferences("audio-suite",0);AudioConfig c=new AudioConfig();c.preset(1);c.profile=1;c.spatial=1;c.buffer=1;c.save(prefs,"headphones");AudioConfig back=AudioConfig.load(prefs,"headphones");require(back.bass==2&&back.eq&&back.spatial==1&&back.buffer==1&&back.gains[1]==2,"Persistência de perfil");require(!AudioConfig.load(prefs,"car").eq,"Perfil vazou para outra saída");prefs.edit().clear().commit();checks.put("profiles_persistence_and_isolation");
            report.put("passed",true);android.util.Log.i("TonalTest","AUDIO_SUITE PASS");
        }catch(Throwable error){try{report.put("passed",false).put("error",error.toString());}catch(JSONException ignored){}android.util.Log.e("TonalTest","AUDIO_SUITE FAIL",error);}
        finally{try{report.put("checks",checks);Files.write(new File(context.getFilesDir(),"audio-suite-result.json").toPath(),report.toString(2).getBytes(java.nio.charset.StandardCharsets.UTF_8));}catch(Exception ignored){}}
    }
}
