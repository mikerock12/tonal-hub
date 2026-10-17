#include <jni.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/replaygain.h>
#include <libswresample/swresample.h>
#include <IAMF_decoder.h>
#include <ebur128.h>
}
namespace {
void check(int code) { if(code<0){char msg[256];av_strerror(code,msg,sizeof msg);throw std::runtime_error(msg);} }
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void fail(JNIEnv* env,const std::exception& e){env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),e.what());}
std::string str(JNIEnv* env,jstring s){const char* p=env->GetStringUTFChars(s,nullptr);std::string r=p?p:"";if(p)env->ReleaseStringUTFChars(s,p);return r;}
AVChannelLayout layout(int channels){AVChannelLayout l{};av_channel_layout_default(&l,channels);if(channels==6){av_channel_layout_uninit(&l);av_channel_layout_from_mask(&l,AV_CH_LAYOUT_5POINT1_BACK);}return l;}
struct Config {
  bool enabled=true,eq=false,compressor=false,hrtf=false;
  std::array<double,10> bands{};
  double bass=0,ceiling=-1,gain=0;
  bool active()const {if(!enabled)return false;if(bass!=0||compressor||gain!=0)return true;if(eq)for(double g:bands)if(g!=0)return true;return false;}
};
struct Processor {
  int rate,channels;bool binaural;
  Config config;
  AVFilterGraph* graph=nullptr;AVFilterContext *input=nullptr,*output=nullptr;
  AVFilterGraph* postGraph=nullptr;AVFilterContext *postInput=nullptr,*postOutput=nullptr;
  AVFrame *frame=av_frame_alloc(),*result=av_frame_alloc();
  std::array<double,14> values{};int ramp=0,postRamp=0;double postCeiling=.891250938;float mix=0,spatialMix=0;
  int64_t pts=0;
  ALCdevice* device=nullptr;ALCcontext* context=nullptr;ALuint source=0;
  std::vector<ALuint> buffers,freeBuffers;
  Processor(int r,int c,bool b):rate(r),channels(c),binaural(b){require(r>=8000&&r<=192000,"Taxa PCM inválida");require(c==1||c==2||c==4||c==6||c==8,"Layout PCM inválido");require(frame&&result,"Sem memória PCM");}
  ~Processor(){avfilter_graph_free(&graph);avfilter_graph_free(&postGraph);av_frame_free(&frame);av_frame_free(&result);closeSpatial();}
  void closeSpatial(){if(context){alcSetThreadContext(context);if(source)alDeleteSources(1,&source);if(!buffers.empty())alDeleteBuffers(int(buffers.size()),buffers.data());alcSetThreadContext(nullptr);alcDestroyContext(context);}if(device)alcCloseDevice(device);context=nullptr;device=nullptr;source=0;buffers.clear();freeBuffers.clear();}
  ALuint nextBuffer(){if(!freeBuffers.empty()){ALuint b=freeBuffers.back();freeBuffers.pop_back();return b;}require(buffers.size()<512,"Fragmentação PCM HRTF excessiva");ALuint b=0;alGenBuffers(1,&b);buffers.push_back(b);return b;}
  ALenum spatialFormat()const{const ALenum formats[]={0,AL_FORMAT_MONO_FLOAT32,AL_FORMAT_STEREO_FLOAT32,0,AL_FORMAT_QUAD32,0,AL_FORMAT_51CHN32,0,AL_FORMAT_71CHN32};return formats[channels];}
  void spatial(){
    if(context)return;
    device=alcLoopbackOpenDeviceSOFT(nullptr);require(device,"OpenAL loopback indisponível");
    const ALCint attrs[]={ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,ALC_FORMAT_TYPE_SOFT,ALC_FLOAT_SOFT,ALC_FREQUENCY,rate,ALC_HRTF_SOFT,ALC_TRUE,0};
    context=alcCreateContext(device,attrs);require(context,"Falha HRTF OpenAL");require(alcSetThreadContext(context),"Falha contexto HRTF");
    ALCint enabled=0;alcGetIntegerv(device,ALC_HRTF_SOFT,1,&enabled);require(enabled==ALC_TRUE,"HRTF não ativada pelo OpenAL");
    alGenSources(1,&source);alSourcei(source,AL_SOURCE_RELATIVE,AL_TRUE);alSourcef(source,AL_ROLLOFF_FACTOR,0);alSource3f(source,AL_POSITION,0,0,channels==1?-1:0);
    alSourcei(source,AL_DIRECT_CHANNELS_SOFT,AL_FALSE);alSourcei(source,AL_SOURCE_SPATIALIZE_SOFT,AL_TRUE);
    if(channels==2){const ALfloat angles[]={float(M_PI/6),float(-M_PI/6)};alSourcefv(source,AL_STEREO_ANGLES,angles);}
    // Keep one short reserve ahead of the mixer. Without it, the source stops at
    // every JNI block and alSourcePlay resets HRTF/filter history on the next block.
    // 256 frames = 5.3 ms at 48 kHz; this is not Bluetooth end-to-end latency.
    ALuint reserve=nextBuffer();std::vector<float> silence(256*channels);
    alBufferData(reserve,spatialFormat(),silence.data(),int(silence.size()*4),rate);alSourceQueueBuffers(source,1,&reserve);
    require(alGetError()==AL_NO_ERROR,"Configuração OpenAL inválida");
    alcSetThreadContext(nullptr);
  }
  void createGraph(){
    graph=avfilter_graph_alloc();require(graph,"Sem memória FFmpeg");graph->nb_threads=1;
    AVFilterContext* previous=nullptr;
    auto add=[&](const char* filter,const std::string& id,const std::string& args,AVFilterContext** save=nullptr){
      const AVFilter* impl=avfilter_get_by_name(filter);require(impl,"Filtro FFmpeg ausente");AVFilterContext* n=nullptr;
      check(avfilter_graph_create_filter(&n,impl,id.c_str(),args.empty()?nullptr:args.c_str(),nullptr,graph));if(previous)check(avfilter_link(previous,0,n,0));previous=n;if(save)*save=n;
    };
    auto l=layout(channels);char name[128];av_channel_layout_describe(&l,name,sizeof name);av_channel_layout_uninit(&l);
    add("abuffer","in","time_base=1/"+std::to_string(rate)+":sample_rate="+std::to_string(rate)+":sample_fmt=flt:channel_layout="+name,&input);
    add("volume","gain","volume=0dB:precision=float");
    const int frequencies[]={31,62,125,250,500,1000,2000,4000,8000,16000};
    for(int b=0;b<10;b++)if(frequencies[b]<rate*.49)add("equalizer","eq"+std::to_string(b),"f="+std::to_string(frequencies[b])+":t=q:w=1.4:g=0");
    add("bass","bass","g=0:f=110:t=q:w=0.707");
    add("acompressor","compressor","threshold=0.125:ratio=2:attack=20:release=250:makeup=1:mix=0");
    values[13]=std::pow(10.,config.ceiling/20);
    add("alimiter","limiter","limit="+std::to_string(values[13])+":level=0:attack=5:release=50:latency=0");
    add("aformat","format","sample_fmts=flt:sample_rates="+std::to_string(rate)+":channel_layouts="+name);
    add("abuffersink","out","",&output);check(avfilter_graph_config(graph,nullptr));ramp=std::max(1,rate*30/1000);
  }
  void update(const Config& c){config=c;ramp=std::max(1,rate*30/1000);postRamp=ramp;}
  void spatialLimit(std::vector<float>& pcm){
    if(!postGraph){
      postGraph=avfilter_graph_alloc();require(postGraph,"Sem memória do limiter espacial");postGraph->nb_threads=1;
      std::string args="time_base=1/"+std::to_string(rate)+":sample_rate="+std::to_string(rate)+":sample_fmt=flt:channel_layout=stereo";
      check(avfilter_graph_create_filter(&postInput,avfilter_get_by_name("abuffer"),"in",args.c_str(),nullptr,postGraph));
      postCeiling=std::pow(10.,config.ceiling/20);
      AVFilterContext* limiter=nullptr;args="limit="+std::to_string(postCeiling)+":level=0:attack=5:release=50:latency=0";
      check(avfilter_graph_create_filter(&limiter,avfilter_get_by_name("alimiter"),"limiter",args.c_str(),nullptr,postGraph));
      check(avfilter_graph_create_filter(&postOutput,avfilter_get_by_name("abuffersink"),"out",nullptr,nullptr,postGraph));
      check(avfilter_link(postInput,0,limiter,0));check(avfilter_link(limiter,0,postOutput,0));check(avfilter_graph_config(postGraph,nullptr));
    }
    std::vector<float> limited;limited.reserve(pcm.size());
    for(size_t offset=0;offset<pcm.size()/2;offset+=256){
      int n=std::min<size_t>(256,pcm.size()/2-offset);
      if(postRamp){postCeiling+=(std::pow(10.,config.ceiling/20)-postCeiling)*std::min(1.,double(n)/postRamp);postRamp=std::max(0,postRamp-n);}
      check(avfilter_graph_send_command(postGraph,"limiter","limit",std::to_string(postCeiling).c_str(),nullptr,0,0));
      av_frame_unref(frame);frame->format=AV_SAMPLE_FMT_FLT;frame->sample_rate=rate;frame->nb_samples=n;frame->ch_layout=layout(2);check(av_frame_get_buffer(frame,0));std::memcpy(frame->data[0],pcm.data()+offset*2,n*2*4);check(av_buffersrc_add_frame_flags(postInput,frame,AV_BUFFERSRC_FLAG_KEEP_REF));
      int ret;
      while((ret=av_buffersink_get_frame(postOutput,result))>=0){require(result->format==AV_SAMPLE_FMT_DBL||result->format==AV_SAMPLE_FMT_FLT,"Formato do limiter espacial");int count=result->nb_samples*2;if(result->format==AV_SAMPLE_FMT_DBL){auto* data=reinterpret_cast<double*>(result->data[0]);for(int i=0;i<count;i++)limited.push_back(float(data[i]));}else{auto* data=reinterpret_cast<float*>(result->data[0]);limited.insert(limited.end(),data,data+count);}av_frame_unref(result);}
      require(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF,"Falha no limiter espacial");
    }
    require(limited.size()==pcm.size(),"Tamanho do limiter espacial");pcm=std::move(limited);
  }
  void commands(int n){
    if(!ramp)return;
    const double a=std::min(1.,double(n)/ramp);
    auto send=[&](const std::string& id,const char* key,double value,bool db=false){check(avfilter_graph_send_command(graph,id.c_str(),key,(std::to_string(value)+(db?"dB":"")).c_str(),nullptr,0,0));};
    auto smooth=[&](int index,double target,const std::string& id,const char* key,bool db=false){double& v=values[index];if(v!=target){v+=(target-v)*a;send(id,key,v,db);}};
    smooth(0,config.enabled?config.gain:0,"gain","volume",true);
    const int frequencies[]={31,62,125,250,500,1000,2000,4000,8000,16000};
    for(int b=0;b<10;b++)if(frequencies[b]<rate*.49)smooth(b+1,config.enabled&&config.eq?config.bands[b]:0,"eq"+std::to_string(b),"gain");
    smooth(11,config.enabled?config.bass:0,"bass","gain");smooth(12,config.enabled&&config.compressor?1:0,"compressor","mix");
    smooth(13,std::pow(10.,config.ceiling/20),"limiter","limit");ramp=std::max(0,ramp-n);
  }
  std::vector<float> process(std::vector<float>& data){
    require(data.size()%channels==0,"Bloco PCM incompleto");for(float& x:data)if(!std::isfinite(x))x=0;
    bool active=config.active();std::vector<float> processed=data;
    if(active||mix>0){
      if(!graph)createGraph();
      for(size_t off=0;off<data.size()/channels;off+=256){
        int n=std::min<size_t>(256,data.size()/channels-off);commands(n);av_frame_unref(frame);frame->format=AV_SAMPLE_FMT_FLT;frame->sample_rate=rate;frame->nb_samples=n;frame->ch_layout=layout(channels);frame->pts=pts;pts+=n;
        check(av_frame_get_buffer(frame,0));std::memcpy(frame->data[0],data.data()+off*channels,n*channels*sizeof(float));check(av_buffersrc_add_frame_flags(input,frame,AV_BUFFERSRC_FLAG_KEEP_REF));
        std::vector<float> wet;int ret;
        while((ret=av_buffersink_get_frame(output,result))>=0){auto p=reinterpret_cast<float*>(result->data[0]);wet.insert(wet.end(),p,p+result->nb_samples*channels);av_frame_unref(result);}
        require(ret==AVERROR(EAGAIN)||ret==AVERROR_EOF,"Falha de saída DSP");require(wet.size()==size_t(n*channels),"DSP alterou o tamanho do bloco");
        for(int f=0;f<n;f++){mix=std::clamp(mix+(active?1.f:-1.f)/std::max(1,rate*30/1000),0.f,1.f);for(int c=0;c<channels;c++){auto i=(off+f)*channels+c;processed[i]=mix==0?data[i]:mix==1?wet[f*channels+c]:data[i]*(1-mix)+wet[f*channels+c]*mix;}}
      }
    }
    int frames=int(data.size()/channels);std::vector<float> stereo(frames*2);
    for(int f=0;f<frames;f++){
      const float* p=processed.data()+f*channels;
      if(channels==1)stereo[f*2]=stereo[f*2+1]=p[0];
      else if(channels==2){stereo[f*2]=p[0];stereo[f*2+1]=p[1];}
      else if(channels==4){stereo[f*2]=(p[0]+.7071f*p[2])/1.7071f;stereo[f*2+1]=(p[1]+.7071f*p[3])/1.7071f;}
      else {float scale=channels==8?3.1213f:2.4142f;stereo[f*2]=(p[0]+.7071f*p[2]+.7071f*p[4]+(channels==8?.7071f*p[6]:0))/scale;stereo[f*2+1]=(p[1]+.7071f*p[2]+.7071f*p[5]+(channels==8?.7071f*p[7]:0))/scale;}
    }
    bool hrtf=config.enabled&&config.hrtf&&!binaural;
    if(hrtf||spatialMix>0){
      spatial();require(alcSetThreadContext(context),"Contexto HRTF indisponível");
      ALint processedCount=0;alGetSourcei(source,AL_BUFFERS_PROCESSED,&processedCount);
      while(processedCount--){ALuint recycled;alSourceUnqueueBuffers(source,1,&recycled);freeBuffers.push_back(recycled);}
      ALuint buffer=nextBuffer();alBufferData(buffer,spatialFormat(),processed.data(),int(processed.size()*4),rate);alSourceQueueBuffers(source,1,&buffer);ALint state;alGetSourcei(source,AL_SOURCE_STATE,&state);if(state!=AL_PLAYING)alSourcePlay(source);
      std::vector<float> rendered(frames*2);alcRenderSamplesSOFT(device,rendered.data(),frames);ALenum err=alGetError();alcSetThreadContext(nullptr);require(err==AL_NO_ERROR,"Falha no render HRTF");
      spatialLimit(rendered);
      for(int f=0;f<frames;f++){spatialMix=std::clamp(spatialMix+(hrtf?1.f:-1.f)/std::max(1,rate*30/1000),0.f,1.f);for(int c=0;c<2;c++){int i=f*2+c;stereo[i]=stereo[i]*(1-spatialMix)+rendered[i]*spatialMix;}}
      if(!hrtf&&spatialMix==0){closeSpatial();avfilter_graph_free(&postGraph);postInput=postOutput=nullptr;}
    }
    for(float& x:stereo)if(!std::isfinite(x))x=0;return stereo;
  }
};
std::atomic<int64_t> prepareGeneration{0};
struct Meter {
  ebur128_state* state;double peak=0;
  Meter(int rate,int channels):state(ebur128_init(channels,rate,EBUR128_MODE_I|EBUR128_MODE_TRUE_PEAK)){
    require(state,"Falha do medidor EBU R128");
    if(channels>=6){ebur128_set_channel(state,3,EBUR128_UNUSED);ebur128_set_channel(state,4,EBUR128_LEFT_SURROUND);ebur128_set_channel(state,5,EBUR128_RIGHT_SURROUND);}
    if(channels==8){ebur128_set_channel(state,6,EBUR128_Mp090);ebur128_set_channel(state,7,EBUR128_Mm090);}
  }
  ~Meter(){ebur128_destroy(&state);}
  void add(const std::vector<float>& pcm,int channels){require(ebur128_add_frames_float(state,pcm.data(),pcm.size()/channels)==EBUR128_SUCCESS,"Falha EBU R128");}
};
struct Wav {
  FILE* file;uint64_t bytes=0;int rate,channels;
  Wav(const std::string& path,int r,int c):file(std::fopen(path.c_str(),"wb")),rate(r),channels(c){require(file,"Sem espaço para o cache PCM");unsigned char header[68]{};if(std::fwrite(header,1,68,file)!=68){std::fclose(file);file=nullptr;throw std::runtime_error("Falha ao escrever WAV");}}
  ~Wav(){if(file)std::fclose(file);}
  void add(const std::vector<float>& pcm){require(bytes+pcm.size()*4<1800ULL*1024*1024,"Arquivo PCM excede 1,8 GB");require(std::fwrite(pcm.data(),4,pcm.size(),file)==pcm.size(),"Cache PCM sem espaço");bytes+=pcm.size()*4;}
  void finish(){
    require(bytes>0,"Arquivo não produziu áudio");unsigned char h[68]{};
    auto put16=[&](int off,uint16_t v){std::memcpy(h+off,&v,2);};auto put32=[&](int off,uint32_t v){std::memcpy(h+off,&v,4);};
    std::memcpy(h,"RIFF",4);put32(4,uint32_t(bytes+60));std::memcpy(h+8,"WAVEfmt ",8);put32(16,40);put16(20,0xfffe);put16(22,channels);put32(24,rate);put32(28,rate*channels*4);put16(32,channels*4);put16(34,32);put16(36,22);put16(38,32);
    auto l=layout(channels);put32(40,uint32_t(l.u.mask));av_channel_layout_uninit(&l);
    const unsigned char guid[]={3,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};std::memcpy(h+44,guid,16);std::memcpy(h+60,"data",4);put32(64,uint32_t(bytes));
    require(std::fseek(file,0,SEEK_SET)==0&&std::fwrite(h,1,68,file)==68&&std::fflush(file)==0,"Falha ao finalizar WAV");
  }
};
std::string metadata(Meter& meter,int rate,int channels,bool binaural,double replayGain,double replayPeak){
  double lufs=-70,peak=0;ebur128_loudness_global(meter.state,&lufs);if(!std::isfinite(lufs))lufs=-70;
  for(int c=0;c<channels;c++){double p=0;ebur128_true_peak(meter.state,c,&p);peak=std::max(peak,p);}
  return "{\"rate\":"+std::to_string(rate)+",\"channels\":"+std::to_string(channels)+",\"binaural\":"+(binaural?"true":"false")+",\"lufs\":"+std::to_string(lufs)+",\"peak\":"+std::to_string(peak)+",\"replayGain\":"+(std::isfinite(replayGain)?std::to_string(replayGain):"null")+",\"replayPeak\":"+std::to_string(replayPeak)+"}";
}
void cancelled(int64_t generation){require(prepareGeneration.load()==generation,"Preparação cancelada");}
std::string prepareIamf(const std::string& input,const std::string& output,int64_t generation){
  std::unique_ptr<FILE,decltype(&std::fclose)> file(std::fopen(input.c_str(),"rb"),std::fclose);require(bool(file),"Arquivo IAMF inacessível");
  auto handle=IAMF_decoder_open();require(handle,"Falha do decoder IAMF");struct Cleanup{IAMF_DecoderHandle h;~Cleanup(){IAMF_decoder_close(h);}}cleanup{handle};
  require(IAMF_decoder_output_layout_set_binaural(handle)==IAMF_OK&&IAMF_decoder_set_bit_depth(handle,32)==IAMF_OK&&IAMF_decoder_set_sampling_rate(handle,48000)==IAMF_OK,"IAMF binaural indisponível");
  IAMF_decoder_set_normalization_loudness(handle,0);IAMF_decoder_peak_limiter_enable(handle,0);IAMF_decoder_enable_head_tracking(handle,0);
  Wav wav(output,48000,2);Meter meter(48000,2);std::vector<uint8_t> pending;std::vector<int32_t> pcm;bool configured=false;int drains=0;
  auto refill=[&]{uint8_t block[65536];size_t n=std::fread(block,1,sizeof block,file.get());pending.insert(pending.end(),block,block+n);require(!std::ferror(file.get()),"Erro de leitura IAMF");};
  for(;;){cancelled(generation);if(pending.size()<65536&&!std::feof(file.get()))refill();
    if(!configured){uint32_t used=0;int ret=IAMF_decoder_configure(handle,pending.data(),uint32_t(pending.size()),&used);require(used<=pending.size(),"IAMF consumiu tamanho inválido");pending.erase(pending.begin(),pending.begin()+used);
      if(ret==IAMF_ERR_BUFFER_TOO_SMALL){require(!std::feof(file.get())&&pending.size()<=4*1024*1024,"Descritores IAMF truncados/excessivos");refill();continue;}
      require(ret==IAMF_OK,"Perfil/codec IAMF não suportado");auto* info=IAMF_decoder_get_stream_info(handle);require(info,"IAMF sem informações");auto size=info->max_frame_size;IAMF_decoder_free_stream_info(info);require(size>0&&size<=65536,"Frame IAMF inválido");pcm.resize(size*2);configured=true;
    }
    bool drain=pending.empty()&&std::feof(file.get());uint32_t used=0;int ret=IAMF_decoder_decode(handle,drain?nullptr:pending.data(),drain?0:int(pending.size()),&used,pcm.data());require(used<=pending.size(),"Tamanho IAMF inválido");pending.erase(pending.begin(),pending.begin()+used);
    if(ret>0){require(size_t(ret)*2<=pcm.size(),"Frame IAMF excessivo");std::vector<float> floats(ret*2);for(size_t i=0;i<floats.size();i++)floats[i]=float(pcm[i]/2147483648.);wav.add(floats);meter.add(floats,2);}
    if(drain){if(ret<=0||++drains>64)break;continue;}
    require(ret>=0||ret==IAMF_ERR_BUFFER_TOO_SMALL||ret==IAMF_ERR_PENDING,"IAMF inválido ou incompatível");
    if(!used){if(std::feof(file.get())){require(pending.empty(),"IAMF truncado");break;}require(pending.size()<=4*1024*1024,"Pacote IAMF excessivo");refill();}
  }
  wav.finish();return metadata(meter,48000,2,true,NAN,0);
}
std::string prepare(const std::string& path,const std::string& output,bool iamf,int64_t generation){
  if(iamf)return prepareIamf(path,output,generation);
  struct Resources {
    AVFormatContext* format=nullptr;AVCodecContext* codec=nullptr;SwrContext* swr=nullptr;AVPacket* packet=av_packet_alloc();AVFrame* frame=av_frame_alloc();
    ~Resources(){avformat_close_input(&format);avcodec_free_context(&codec);swr_free(&swr);av_packet_free(&packet);av_frame_free(&frame);}
  }r;
  require(r.packet&&r.frame,"Sem memória do decoder");check(avformat_open_input(&r.format,path.c_str(),nullptr,nullptr));check(avformat_find_stream_info(r.format,nullptr));
  for(unsigned i=0;i<r.format->nb_stream_groups;i++)require(r.format->stream_groups[i]->type!=AV_STREAM_GROUP_PARAMS_IAMF_AUDIO_ELEMENT&&r.format->stream_groups[i]->type!=AV_STREAM_GROUP_PARAMS_IAMF_MIX_PRESENTATION,"IAMF em MP4 não suportado: use um arquivo .iamf bruto");
  const AVCodec* decoder=nullptr;int track=av_find_best_stream(r.format,AVMEDIA_TYPE_AUDIO,-1,-1,&decoder,0);check(track);
  r.codec=avcodec_alloc_context3(decoder);require(r.codec,"Sem memória do codec");check(avcodec_parameters_to_context(r.codec,r.format->streams[track]->codecpar));r.codec->thread_count=1;check(avcodec_open2(r.codec,decoder,nullptr));
  int channels=r.codec->ch_layout.nb_channels,rate=r.codec->sample_rate;require(channels==1||channels==2||channels==4||channels==6||channels==8,"Use mono, estéreo, quad, 5.1 ou 7.1");require(rate>=8000&&rate<=192000,"Taxa de áudio fora de 8–192 kHz");
  AVChannelLayout dest=layout(channels);check(swr_alloc_set_opts2(&r.swr,&dest,AV_SAMPLE_FMT_FLT,rate,&r.codec->ch_layout,r.codec->sample_fmt,rate,0,nullptr));av_channel_layout_uninit(&dest);check(swr_init(r.swr));
  double replayGain=NAN,replayPeak=0;
  const auto* rg=av_packet_side_data_get(r.format->streams[track]->codecpar->coded_side_data,r.format->streams[track]->codecpar->nb_coded_side_data,AV_PKT_DATA_REPLAYGAIN);
  if(rg&&rg->size>=sizeof(AVReplayGain)){const auto* data=reinterpret_cast<const AVReplayGain*>(rg->data);if(data->track_gain!=INT32_MIN)replayGain=data->track_gain/100000.;replayPeak=data->track_peak/100000.;}
  for(AVDictionary* dict:{r.format->metadata,r.format->streams[track]->metadata}){
    auto* gain=av_dict_get(dict,"REPLAYGAIN_TRACK_GAIN",nullptr,0);if(gain){char* end=nullptr;double value=std::strtod(gain->value,&end);if(end!=gain->value&&std::isfinite(value))replayGain=std::clamp(value,-24.,12.);}
    auto* peak=av_dict_get(dict,"REPLAYGAIN_TRACK_PEAK",nullptr,0);if(peak){double value=std::strtod(peak->value,nullptr);if(std::isfinite(value)&&value>0)replayPeak=value;}
  }
  Wav wav(output,rate,channels);Meter meter(rate,channels);
  auto receive=[&]{int ret;while((ret=avcodec_receive_frame(r.codec,r.frame))>=0){cancelled(generation);require(r.frame->sample_rate==rate&&r.frame->ch_layout.nb_channels==channels,"Formato de áudio mudou durante a faixa");int capacity=swr_get_out_samples(r.swr,r.frame->nb_samples);check(capacity);std::vector<float> pcm(capacity*channels);uint8_t* out=reinterpret_cast<uint8_t*>(pcm.data());int n=swr_convert(r.swr,&out,capacity,const_cast<const uint8_t**>(r.frame->extended_data),r.frame->nb_samples);check(n);pcm.resize(n*channels);for(float& x:pcm)if(!std::isfinite(x))x=0;wav.add(pcm);meter.add(pcm,channels);av_frame_unref(r.frame);}require(ret==AVERROR_EOF||ret==AVERROR(EAGAIN),"Arquivo de áudio danificado");};
  int read;
  while((read=av_read_frame(r.format,r.packet))>=0){cancelled(generation);if(r.packet->stream_index==track){check(avcodec_send_packet(r.codec,r.packet));receive();}av_packet_unref(r.packet);}
  require(read==AVERROR_EOF,"Erro ao ler o arquivo");check(avcodec_send_packet(r.codec,nullptr));receive();wav.finish();return metadata(meter,rate,channels,false,replayGain,replayPeak);
}
} // namespace
extern "C" JNIEXPORT jlong JNICALL Java_app_tonal_android_NativeAudio_create(JNIEnv* env,jclass,jint rate,jint channels,jboolean binaural){try{av_log_set_level(AV_LOG_ERROR);return reinterpret_cast<jlong>(new Processor(rate,channels,binaural));}catch(const std::exception& e){fail(env,e);return 0;}}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_NativeAudio_destroy(JNIEnv*,jclass,jlong handle){delete reinterpret_cast<Processor*>(handle);}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_NativeAudio_configure(JNIEnv* env,jclass,jlong handle,jfloatArray array){try{require(env->GetArrayLength(array)==17,"Configuração DSP inválida");float a[17];env->GetFloatArrayRegion(array,0,17,a);for(float v:a)require(std::isfinite(v),"Parâmetro DSP inválido");Config c;c.enabled=a[0]!=0;c.eq=a[1]!=0;for(int i=0;i<10;i++)c.bands[i]=std::clamp(double(a[i+2]),-12.,12.);c.bass=std::clamp(double(a[12]),0.,12.);c.compressor=a[13]!=0;c.ceiling=std::clamp(double(a[14]),-6.,-.1);c.gain=std::clamp(double(a[15]),-24.,12.);c.hrtf=a[16]!=0;reinterpret_cast<Processor*>(handle)->update(c);}catch(const std::exception& e){fail(env,e);}}
extern "C" JNIEXPORT jfloatArray JNICALL Java_app_tonal_android_NativeAudio_process(JNIEnv* env,jclass,jlong handle,jfloatArray array){try{std::vector<float> data(env->GetArrayLength(array));env->GetFloatArrayRegion(array,0,data.size(),data.data());auto output=reinterpret_cast<Processor*>(handle)->process(data);auto result=env->NewFloatArray(output.size());if(result)env->SetFloatArrayRegion(result,0,output.size(),output.data());return result;}catch(const std::exception& e){fail(env,e);return nullptr;}}
extern "C" JNIEXPORT jstring JNICALL Java_app_tonal_android_NativeAudio_prepare(JNIEnv* env,jclass,jstring path,jstring output,jboolean iamf,jlong generation){try{auto text=prepare(str(env,path),str(env,output),iamf,generation);return env->NewStringUTF(text.c_str());}catch(const std::exception& e){fail(env,e);return nullptr;}}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_NativeAudio_cancel(JNIEnv*,jclass,jlong generation){prepareGeneration.store(generation);}
