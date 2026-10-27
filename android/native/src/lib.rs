use jni::{JNIEnv, JavaVM, objects::{GlobalRef, JClass, JString, JValue}, sys::{jboolean, jint, jlong}};
use librespot::{
    connect::{ConnectConfig, LoadRequest, LoadRequestOptions, PlayingTrack, Spirc},
    core::{authentication::Credentials, cache::Cache, config::SessionConfig, session::Session},
    playback::{audio_backend::{Sink, SinkError, SinkResult}, config::{Bitrate, PlayerConfig, NormalisationMethod},
        convert::Converter, decoder::AudioPacket, mixer::{self, MixerConfig}, player::{Player, PlayerEvent}},
};
use std::{sync::{OnceLock, Mutex, atomic::{AtomicU64, Ordering}}, time::Duration};
use tokio::sync::mpsc::{UnboundedSender, unbounded_channel};

static JVM: OnceLock<JavaVM> = OnceLock::new();
static BRIDGE: OnceLock<GlobalRef> = OnceLock::new();
static COMMAND: Mutex<Option<UnboundedSender<String>>> = Mutex::new(None);
static GENERATION: AtomicU64 = AtomicU64::new(0);
static LOGGER: EngineLogger = EngineLogger;
struct EngineLogger;
impl log::Log for EngineLogger {
    fn enabled(&self, record: &log::Metadata) -> bool {
        record.level()<=log::Level::Warn || record.target().ends_with("spirc")
    }
    fn log(&self, record: &log::Record) {
        if !self.enabled(record.metadata()) {return;}
        let message=format!("{}: {}",record.target(),record.args());
        // Library diagnostics can include URLs/identifiers. Keep long credential-like
        // runs out of logcat, and never enable protocol packet or token debug logs.
        let mut safe=String::new();let mut run=String::new();
        for c in message.chars().take(2048).chain(std::iter::once(' ')) {
            if c.is_ascii_alphanumeric() || "_-+/=".contains(c) {run.push(c);} else {
                safe.push_str(if run.len()>=32{"<redacted>"}else{&run});run.clear();safe.push(c);
            }
        }
        if let Some(vm)=JVM.get() {if let Ok(env)=vm.attach_current_thread_permanently() {if let Ok(s)=env.new_string(safe) {let s=env.auto_local(s);let _=call_void("onDiagnostic","(Ljava/lang/String;)V",&[JValue::Object(s.as_ref())]);}}}
    }
    fn flush(&self) {}
}

fn call_void(name: &str, sig: &str, args: &[JValue]) -> Result<(), String> {
    let mut env = JVM.get().ok_or("JVM unavailable")?.attach_current_thread_permanently().map_err(|e|e.to_string())?;
    let class: &JClass = BRIDGE.get().ok_or("Bridge unavailable")?.as_obj().into();
    let result = env.call_static_method(class, name, sig, args).map_err(|e|e.to_string());
    if env.exception_check().unwrap_or(false) { let _ = env.exception_clear(); return Err("Audio Android failure".into()); }
    result.map(|_| ())
}
fn status(generation: u64, text: &str) {
    if generation != GENERATION.load(Ordering::SeqCst) {return;}
    if let Some(vm) = JVM.get() {
        if let Ok(env) = vm.attach_current_thread_permanently() {
            if let Ok(s) = env.new_string(text) {
                let s = env.auto_local(s);
                let _ = call_void("onNativeStatus", "(JLjava/lang/String;)V", &[JValue::Long(generation as jlong), JValue::Object(s.as_ref())]);
            }
        }
    }
}
fn jam_result(generation:u64,value:&serde_json::Value){
    if generation!=GENERATION.load(Ordering::SeqCst){return;}
    if let Some(vm)=JVM.get(){if let Ok(env)=vm.attach_current_thread_permanently(){if let Ok(s)=env.new_string(value.to_string()){let s=env.auto_local(s);let _=call_void("onJamResult","(JLjava/lang/String;)V",&[JValue::Long(generation as jlong),JValue::Object(s.as_ref())]);}}}
}
fn pcm(samples: &[f32]) -> SinkResult<()> {
    let env = JVM.get().unwrap().attach_current_thread_permanently()
        .map_err(|e|SinkError::OnWrite(e.to_string()))?;
    let array = env.new_float_array(samples.len() as i32).map_err(|e|SinkError::OnWrite(e.to_string()))?;
    let array = env.auto_local(array);
    env.set_float_array_region(&array, 0, samples).map_err(|e|SinkError::OnWrite(e.to_string()))?;
    call_void("onPcm", "([F)V", &[JValue::Object(array.as_ref())]).map_err(SinkError::OnWrite)
}
struct AndroidSink { generation: u64 }
impl Sink for AndroidSink {
    fn start(&mut self) -> SinkResult<()> {
        if self.generation != GENERATION.load(Ordering::SeqCst) { return Err(SinkError::NotConnected("Stopped".into())); }
        call_void("onAudioStart", "(I)V", &[JValue::Int(44100)]).map_err(SinkError::OnWrite)
    }
    fn stop(&mut self) -> SinkResult<()> {
        if self.generation == GENERATION.load(Ordering::SeqCst) { let _ = call_void("onAudioPause", "()V", &[]); }
        Ok(())
    }
    fn write(&mut self, packet: AudioPacket, _: &mut Converter) -> SinkResult<()> {
        if self.generation != GENERATION.load(Ordering::SeqCst) { return Err(SinkError::NotConnected("Stopped".into())); }
        if let AudioPacket::Samples(samples) = packet {
            let floats: Vec<f32> = samples.iter().map(|x|*x as f32).collect();
            pcm(&floats)?;
        }
        Ok(())
    }
}
fn init(env: &mut JNIEnv, class: JClass) {
    JVM.get_or_init(||env.get_java_vm().expect("JavaVM"));
    BRIDGE.get_or_init(||env.new_global_ref(class).expect("class reference"));
    static LOG_INIT: OnceLock<()> = OnceLock::new();
    LOG_INIT.get_or_init(||{
        let _=log::set_logger(&LOGGER);log::set_max_level(log::LevelFilter::Debug);
        let _=librespot::audio::AudioFetchParams::set(librespot::audio::AudioFetchParams{
            read_ahead_before_playback:Duration::from_secs(2),
            read_ahead_during_playback:Duration::from_secs(60),
            ..Default::default()
        });
    });
}
fn text(env: &mut JNIEnv, value: &JString) -> String { env.get_string(value).map(|s|s.into()).unwrap_or_default() }
fn next_generation() -> u64 {
    if let Some(tx) = COMMAND.lock().unwrap_or_else(|poisoned|poisoned.into_inner()).take() { let _ = tx.send("stop".into()); }
    let generation=GENERATION.fetch_add(1, Ordering::SeqCst) + 1;
    let _=call_void("onGeneration","(J)V",&[JValue::Long(generation as jlong)]);
    generation
}
#[unsafe(no_mangle)]
pub extern "system" fn Java_app_tonal_android_NativeBridge_nativeStart(mut env: JNIEnv, class: JClass, token: JString, cache: JString, bitrate: jint, normalize: jboolean) {
    init(&mut env, class);
    let token = text(&mut env, &token); let cache = text(&mut env, &cache);
    let generation = next_generation();
    let (tx, mut rx) = unbounded_channel::<String>();
    *COMMAND.lock().unwrap_or_else(|poisoned|poisoned.into_inner()) = Some(tx);
    std::thread::spawn(move || {
        let panic=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let runtime = tokio::runtime::Builder::new_multi_thread().worker_threads(2).enable_all().build().unwrap();
        runtime.block_on(async move {
            status(generation, "Conectando ao Spotify…");
            let result = async {
                let mut session_config = SessionConfig::default();
                // The audio engine has its own desktop OAuth authorization.
                // Developer Web API tokens must never be used for this session.
                session_config.client_id = "65b708073fc0480ea92a077233ca87bd".into();
                let device_file = std::path::Path::new(&cache).join("device-id");
                if let Ok(id)=std::fs::read_to_string(&device_file) {
                    if !id.trim().is_empty() {session_config.device_id=id.trim().to_owned();}
                } else {let _=std::fs::write(&device_file,&session_config.device_id);}
                session_config.tmp_dir=std::path::Path::new(&cache).join("tmp");
                std::fs::create_dir_all(&session_config.tmp_dir)?;
                let audio_cache=format!("{cache}/audio");
                let cache = Cache::new(Some(&cache), None, Some(&audio_cache), Some(256 * 1024 * 1024))?;
                let credentials=cache.credentials().or_else(||if token.is_empty(){None}else{Some(Credentials::with_access_token(token))})
                    .ok_or_else(||librespot::core::Error::unauthenticated("Autorize a reprodução no navegador."))?;
                let session = Session::new(session_config, Some(cache));
                let config = PlayerConfig { bitrate: match bitrate {96=>Bitrate::Bitrate96,160=>Bitrate::Bitrate160,_=>Bitrate::Bitrate320}, normalisation: normalize != 0, normalisation_method:NormalisationMethod::Basic, position_update_interval:Some(Duration::from_secs(1)), ditherer: None, ..Default::default() };
                let mixer = mixer::find(None).unwrap()(MixerConfig::default())?;
                // Android/car media volume is the volume control. No hidden second attenuator.
                let player = Player::new(config, session.clone(), Box::new(mixer::NoOpVolume), move ||Box::new(AndroidSink{generation}));
                let connect = ConnectConfig {name:"Tonalify Android".into(), initial_volume:u16::MAX,disable_volume:true, ..Default::default()};
                let events=player.get_player_event_channel();
                let (spirc,task)=Spirc::new(connect, session.clone(), credentials, player, mixer).await?;
                Ok::<_,librespot::core::Error>((spirc,task,events,session))
            };
            let connection = tokio::time::timeout(Duration::from_secs(40), result);
            tokio::pin!(connection);
            let (spirc, task, mut events,jam_session) = loop {
                tokio::select! {
                    result = &mut connection => match result {
                        Ok(Ok(pair)) => break pair,
                        Ok(Err(error)) => {status(generation, &format!("Falha Spotify: {error}")); return;},
                        Err(_) => {status(generation, "Tempo de conexão esgotado; tente reconectar."); return;}
                    },
                    cmd = rx.recv() => if cmd.as_deref() == Some("stop") || cmd.is_none() {return;}
                }
            };
            if generation != GENERATION.load(Ordering::SeqCst) {let _=spirc.shutdown(); return;}
            let _ = spirc.activate();
            status(generation, "Pronto • Spotify Connect: Tonalify Android");
            tokio::pin!(task);
            let jam_busy=std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false));
            // Audio keys can stop arriving on a half-broken session: librespot then
            // skips every track in a few seconds. Two tracks lost to key timeouts
            // within 30 s end the cascade: report the first one as the resume point
            // and let the service reconnect, instead of sweeping the whole queue.
            // Tracks that are really unavailable (no key timeout) are skipped as before.
            let mut key_failures=0u32;let mut first_failure:Option<std::time::Instant>=None;
            let mut seen_timeouts=librespot::core::audio_key::KEY_TIMEOUTS.load(Ordering::Relaxed);
            loop {
                tokio::select! {
                    _ = &mut task => {if generation==GENERATION.load(Ordering::SeqCst){status(generation, "Sessão Spotify encerrada. Toque na música para reconectar.");}break;},
                    Some(event) = events.recv() => {
                        let mut progress_override=None;
                        let key_timed_out=matches!(event,PlayerEvent::Unavailable{..})&&{let t=librespot::core::audio_key::KEY_TIMEOUTS.load(Ordering::Relaxed);let changed=t!=seen_timeouts;seen_timeouts=t;changed};
                        match &event {
                            PlayerEvent::Unavailable{track_id,..} if key_timed_out => {
                                let now=std::time::Instant::now();
                                if first_failure.map_or(true,|at|now.duration_since(at)>Duration::from_secs(30)) {first_failure=Some(now);key_failures=0;progress_override=Some((track_id.clone(),0u32,true));}
                                key_failures+=1;
                                if key_failures>=2 && generation==GENERATION.load(Ordering::SeqCst) {key_failures=0;first_failure=None;status(generation,"Falha Spotify: músicas indisponíveis em sequência; reconectando sem pular a fila.");}
                            },
                            PlayerEvent::Playing{..} => {key_failures=0;first_failure=None;},
                            // Jam membership pushed by the service (see spirc handle_session_update).
                            PlayerEvent::SessionClientChanged{client_id,client_name,..} if client_id=="tonal-jam" && client_name.len()<=65536 => {
                                if let Ok(update)=serde_json::from_str::<serde_json::Value>(client_name){jam_result(generation,&serde_json::json!({"event":"session_update","update":update}));}
                            },
                            _ => {}
                        }
                        let progress=progress_override.or(match event {
                            PlayerEvent::Playing{track_id,position_ms,..}|PlayerEvent::PositionChanged{track_id,position_ms,..}|PlayerEvent::Seeked{track_id,position_ms,..}=>Some((track_id,position_ms,true)),
                            PlayerEvent::Paused{track_id,position_ms,..}=>Some((track_id,position_ms,false)),
                            _=>None,
                        });
                        if generation==GENERATION.load(Ordering::SeqCst) {if let Some((track,position,active))=progress {if let Some(vm)=JVM.get(){if let Ok(env)=vm.attach_current_thread_permanently(){if let Ok(uri)=env.new_string(track.to_uri().unwrap_or_default()){let uri=env.auto_local(uri);let _=call_void("onProgress","(JLjava/lang/String;IZ)V",&[JValue::Long(generation as jlong),JValue::Object(uri.as_ref()),JValue::Int(position.min(i32::MAX as u32) as i32),JValue::Bool(active as u8)]);}}}}}
                    },
                    cmd = rx.recv() => {
                        let Some(cmd) = cmd else {let _=spirc.shutdown();break;};
                        if let Some(payload)=cmd.strip_prefix("jam|"){
                            if payload.len()>8192{jam_result(generation,&serde_json::json!({"status":413,"error":"command_too_large"}));continue;}
                            let Ok(mut command)=serde_json::from_str::<serde_json::Value>(payload) else {jam_result(generation,&serde_json::json!({"status":400,"error":"invalid_json"}));continue;};
                            if jam_busy.swap(true,Ordering::SeqCst){jam_result(generation,&serde_json::json!({"status":409,"error":"busy","request":command.get("request")}));continue;}
                            if let Some(object)=command.as_object_mut(){object.insert("device".into(),serde_json::Value::String(jam_session.device_id().to_owned()));}
                            let session=jam_session.clone();let busy=jam_busy.clone();
                            // A network await never blocks the Spirc pause/stop command loop.
                            tokio::spawn(async move{let result=tonal_jam_transport::execute(&session,&command).await;busy.store(false,Ordering::SeqCst);jam_result(generation,&result);});
                            continue;
                        }
                        let result = match cmd.as_str() {
                            "stop" => {let _=spirc.shutdown();break;},
                            "play"=>spirc.play(), "pause"=>spirc.pause(), "toggle"=>spirc.play_pause(),
                            "next"=>spirc.next(), "prev"=>spirc.prev(),
                            "shuffle:on"=>spirc.shuffle(true), "shuffle:off"=>spirc.shuffle(false),
                            "repeat:on"=>spirc.repeat(true), "repeat:off"=>spirc.repeat(false),
                            "repeat-track:on"=>spirc.repeat_track(true), "repeat-track:off"=>spirc.repeat_track(false),
                            // requeue keeps the audible decoder; restore reloads it.
                            _ if cmd.starts_with("restore|") || cmd.starts_with("requeue|") || cmd.starts_with("editqueue|") => {
                                let mut parts=cmd.splitn(4,'|');let kind=parts.next();let edit_only=kind==Some("editqueue");let keep_playing_track=edit_only||kind==Some("requeue");
                                let Some(position)=parts.next().and_then(|s|s.parse::<u32>().ok()) else {continue;};
                                let track=parts.next().unwrap_or_default();let selection=parts.next().unwrap_or_default();
                                let mut options=LoadRequestOptions{start_playing:!edit_only,seek_to:position,keep_playing_track,queue_edit_only:edit_only,..Default::default()};
                                if !track.is_empty(){options.playing_track=Some(PlayingTrack::Uri(track.to_owned()));}
                                if selection.starts_with("queue|")||selection.starts_with("context|") {
                                    let mut selection_parts=selection.splitn(3,'|');let kind=selection_parts.next().unwrap_or_default();
                                    let Some(index)=selection_parts.next().and_then(|s|s.parse::<u32>().ok()) else {continue;};let source=selection_parts.next().unwrap_or_default();
                                    if options.playing_track.is_none(){options.playing_track=Some(PlayingTrack::Index(index));}
                                    if kind=="context"{spirc.load(LoadRequest::from_context_uri(source.to_owned(),options))}
                                    else {let tracks:Vec<String>=source.split(',').filter(|s|s.starts_with("spotify:track:")).take(500).map(str::to_owned).collect();if tracks.is_empty(){continue;}spirc.load(LoadRequest::from_tracks(tracks,options))}
                                } else if selection.starts_with("spotify:album:")||selection.starts_with("spotify:playlist:") {spirc.load(LoadRequest::from_context_uri(selection.to_owned(),options))}
                                else {let uri=if track.is_empty(){selection}else{track};if !uri.starts_with("spotify:track:"){continue;}spirc.load(LoadRequest::from_tracks(vec![uri.to_owned()],options))}
                            },
                            _ if cmd.starts_with("seek|") => {
                                let Ok(ms)=cmd[5..].parse::<u32>() else {continue;};
                                spirc.set_position_ms(ms)
                            },
                            _ if cmd.starts_with("queue|") || cmd.starts_with("context|") => {
                                let mut parts=cmd.splitn(3,'|');
                                let kind=parts.next().unwrap_or_default();
                                let Some(index)=parts.next().and_then(|s|s.parse::<u32>().ok()) else {continue;};
                                let Some(uris)=parts.next() else {continue;};
                                let options=LoadRequestOptions{start_playing:true,playing_track:Some(PlayingTrack::Index(index)),..Default::default()};
                                if kind=="context" {
                                    if !uris.starts_with("spotify:album:") && !uris.starts_with("spotify:playlist:") {continue;}
                                    spirc.load(LoadRequest::from_context_uri(uris.to_owned(),options))
                                } else {
                                    let tracks:Vec<String>=uris.split(',').filter(|s|s.starts_with("spotify:track:")).take(500).map(str::to_owned).collect();
                                    if tracks.is_empty() || index as usize>=tracks.len() {continue;}
                                    spirc.load(LoadRequest::from_tracks(tracks,options))
                                }
                            },
                            _ if cmd.starts_with("spotify:") => spirc.load(if cmd.starts_with("spotify:track:") {
                                LoadRequest::from_tracks(vec![cmd], LoadRequestOptions{start_playing:true,..Default::default()})
                            } else {LoadRequest::from_context_uri(cmd,LoadRequestOptions{start_playing:true,..Default::default()})}),
                            _=>continue,
                        };
                        if let Err(error)=result {status(generation, &format!("Comando indisponível: {error}"));}
                    }
                }
            }
        });
        }));
        if panic.is_err(){status(generation,"Falha no motor nativo; recuperando sessão.");}
    });
}
#[unsafe(no_mangle)]
pub extern "system" fn Java_app_tonal_android_NativeBridge_sendCommand(mut env: JNIEnv, _: JClass, command: JString) {
    let command=text(&mut env,&command);
    if command == "stop" {next_generation();return;}
    if let Some(tx)=COMMAND.lock().unwrap_or_else(|poisoned|poisoned.into_inner()).as_ref() {let _=tx.send(command);}
}
#[unsafe(no_mangle)]
pub extern "system" fn Java_app_tonal_android_NativeBridge_playFile(mut env: JNIEnv, class: JClass, path: JString) {
    init(&mut env,class); let path=text(&mut env,&path); let generation=next_generation();
    let(tx,mut rx)=unbounded_channel::<String>(); *COMMAND.lock().unwrap_or_else(|poisoned|poisoned.into_inner())=Some(tx);
    std::thread::spawn(move || {
        let panic=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        use symphonia::core::{audio::SampleBuffer, codecs::DecoderOptions, formats::FormatOptions, io::MediaSourceStream, meta::MetadataOptions, probe::Hint};
        let result=(||->Result<(),Box<dyn std::error::Error>> {
            let file=std::fs::File::open(path)?;
            let stream=MediaSourceStream::new(Box::new(file),Default::default());
            let mut format=symphonia::default::get_probe().format(&Hint::new(),stream,&FormatOptions::default(),&MetadataOptions::default())?.format;
            let track=format.default_track().ok_or("Faixa de áudio ausente")?;
            let id=track.id; let mut decoder=symphonia::default::get_codecs().make(&track.codec_params,&DecoderOptions::default())?;
            let mut paused=false; let mut rate=0; let mut channels=0;
            status(generation, "Arquivo local • decodificação nativa Rust");
            loop {
                if generation!=GENERATION.load(Ordering::SeqCst) {break;}
                while let Ok(cmd)=rx.try_recv(){match cmd.as_str(){"stop"=>return Ok(()),"toggle"=>paused=!paused,"pause"=>paused=true,"play"=>paused=false,_=>{}};let _=call_void(if paused{"onAudioPause"}else{"onAudioResume"},"()V",&[]);}
                if paused {std::thread::sleep(Duration::from_millis(5));continue;}
                let packet=match format.next_packet(){Ok(p)=>p,Err(symphonia::core::errors::Error::IoError(e)) if e.kind()==std::io::ErrorKind::UnexpectedEof=>break,Err(e)=>return Err(e.into())};
                if packet.track_id()!=id {continue;}
                let decoded=decoder.decode(&packet)?; let spec=*decoded.spec();
                if ![1,2,4,6,8].contains(&spec.channels.count()) {return Err("Layout de canais não suportado".into());}
                if rate!=spec.rate || channels!=spec.channels.count() {rate=spec.rate;channels=spec.channels.count();call_void("onAudioFormat","(II)V",&[JValue::Int(rate as i32),JValue::Int(channels as i32)])?;}
                let mut samples=SampleBuffer::<f32>::new(decoded.capacity() as u64,spec);samples.copy_interleaved_ref(decoded);
                pcm(samples.samples())?;
            }
            Ok(())
        })();
        if generation==GENERATION.load(Ordering::SeqCst){match result{Ok(_)=>{let _=call_void("onAudioEnd","()V",&[]);status(generation, "Arquivo finalizado")},Err(error)=>{let _=call_void("onAudioPause","()V",&[]);status(generation, &format!("Erro de arquivo: {error}"))}};}
        }));
        if panic.is_err(){status(generation,"Falha no decodificador de arquivo.");}
    });
}
