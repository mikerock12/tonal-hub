package app.tonal.android;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.Consumer;
import org.json.*;

public final class MainActivity extends Activity {
    private static final int BG=Color.rgb(17,18,20),CARD=Color.rgb(31,33,36),TEXT=Color.rgb(245,245,245),MUTED=Color.rgb(168,174,182),BLUE=Color.rgb(0,154,211);
    private LinearLayout root,body;
    private TextView status,now,byline,time;
    private ImageButton toggle,favoriteButton;
    private ImageView nowCover;
    private ProgressBar progress;
    private Ui ui;
    private Artwork artwork;private MusicMetadata musicMetadata;
    private CatalogScreen catalog;
    private final ArrayList<TextView> tabs=new ArrayList<>();
    private JSONObject currentTrack;
    private Runnable pendingPlayback;
    private final Consumer<String> commandObserver=command->{if(command.equals("pause")){pendingPlayback=null;}if(command.equals("pause")||command.equals("toggle")||command.equals("next")||command.equals("prev")||command.startsWith("seek|"))cancelMix();};
    private boolean connecting,diagnostics,shuffle,repeat,clockPlaying;
    private int positionMs,durationMs,playbackRevision;
    private String expectedUri="",engineDeviceId="";
    private long expectedUntil;
    private long positionAt;
    private SpotifyClient api,engineApi;private ListeningMix listeningMix;
    private final Handler handler=new Handler(Looper.getMainLooper());
    
    private boolean destroyed,polling,local,authorizingEngine,foreground;
    private long lastPoll;
    private String currentUri="",queriedLikeUri="";
    private final Consumer<String> statusListener=value->{if(clockPlaying)positionMs=currentPosition();positionAt=SystemClock.elapsedRealtime();clockPlaying=NativeBridge.playing;if(status!=null)status.setText(value);if(value.startsWith("Pronto •")){connecting=false;if(pendingPlayback!=null){Runnable run=pendingPlayback;pendingPlayback=null;run.run();}}if(value.startsWith("Falha")||value.startsWith("Tempo")||value.startsWith("Sessão")){connecting=false;}if(value.startsWith("Reconectando"))connecting=true;updateTransport();};
    private final Runnable ticker=new Runnable(){public void run(){
        if(destroyed)return;
        updateTransport();
        if(!local&&NativeBridge.ready&&api.loggedIn()&&!polling&&SystemClock.elapsedRealtime()-lastPoll>3000){
            polling=true;lastPoll=SystemClock.elapsedRealtime();
            int revision=playbackRevision;api.get("me/player",json->{polling=false;if(revision==playbackRevision)updatePlayer(json);},error->{polling=false;});
        }
        handler.postDelayed(this,diagnostics?2500:NativeBridge.playing||api.loggedIn()?1000:5000);
    }};
    private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
    private TextView label(String text,int size,int color){TextView v=new TextView(this);v.setText(text);v.setTextSize(size);v.setTextColor(color);v.setPadding(0,dp(4),0,dp(4));return v;}
    @Override public void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(BG);
        importTestSession();
        api=new SpotifyClient(this);engineApi=new SpotifyClient(this,true);ui=new Ui(this);artwork=new Artwork(this);
        diagnostics=BuildConfig.DEBUG&&(getIntent().getBooleanExtra("benchmark",false)||getIntent().getBooleanExtra("selfTest",false)||getIntent().getBooleanExtra("streamingBenchmark",false)||getIntent().getBooleanExtra("catalogTest",false));
        buildShell();catalog=new CatalogScreen(this,api,artwork,body);
        setContentView(root);if(BuildConfig.DEBUG&&getIntent().getBooleanExtra("mixUiFixture",false))handler.post(this::mixUiFixture);if(BuildConfig.DEBUG&&getIntent().getBooleanExtra("openTextPlaylist",false))handler.post(()->TextPlaylistWizard.show(this,api));NativeBridge.listeners.add(statusListener);NativeBridge.commandListeners.add(commandObserver);home();String literal=getIntent().getStringExtra("literalQuery");if(literal!=null)catalog.search(literal);
        if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,()->{if(!catalog.back())finish();});
        if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=android.content.pm.PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},10);
        loadDsp();
        if(BuildConfig.DEBUG&&getIntent().getBooleanExtra("audioSuite",false))api.worker.execute(()->AudioSelfTest.run(this));
        if(BuildConfig.DEBUG&&getIntent().getBooleanExtra("streamingBenchmark",false)){
            String uri=getIntent().getStringExtra("trackUri");
            Consumer<String> autoPlay=new Consumer<>(){public void accept(String value){if(value.startsWith("Pronto •")){NativeBridge.listeners.remove(this);if(uri!=null)NativeBridge.command(uri);}}};
            NativeBridge.listeners.add(autoPlay);connectSpotify();
        }
        if(BuildConfig.DEBUG&&(getIntent().getBooleanExtra("selfTest",false)||getIntent().getBooleanExtra("benchmark",false))){
            boolean effects=getIntent().getBooleanExtra("effects",false);
            api.worker.execute(()->{try{Dsp.selfTest();android.util.Log.i("TonalTest","DSP_SELF_TEST PASS");
                if(effects){float[] gains={3,2,0,-1,0,1,2,3,2,1};NativeBridge.audio.dsp.settings=new Dsp.Settings(gains,3,true);}
                File test=copyAsset();runOnUiThread(()->playLocal(test,"Ensaio Vorbis • 60 segundos"));
            }catch(Exception|AssertionError error){android.util.Log.e("TonalTest","SELF_TEST FAIL",error);runOnUiThread(()->status.setText(error.toString()));}});
        }
    }
    private void importTestSession(){
        if(!BuildConfig.DEBUG)return;
        File input=new File(getFilesDir(),"test-auth.json");if(!input.isFile())return;
        try{
            JSONObject seed=new JSONObject(new String(java.nio.file.Files.readAllBytes(input.toPath()),StandardCharsets.UTF_8));
            new Secrets(this).save(seed.getJSONObject("tokens"));
            getSharedPreferences("settings",0).edit().putString("clientId",seed.getString("clientId")).apply();
            File cache=new File(getFilesDir(),SpotifyClient.ENGINE_CACHE);cache.mkdirs();
            if(seed.has("credentials"))java.nio.file.Files.write(new File(cache,"credentials.json").toPath(),seed.getJSONObject("credentials").toString().getBytes(StandardCharsets.UTF_8));
            android.util.Log.i("TonalTest","SESSION_IMPORT PASS");
        }catch(Exception error){android.util.Log.e("TonalTest","SESSION_IMPORT FAILED (details withheld)");}
        finally{input.delete();}
    }
    private void home(){catalog.home();}
    private void search(){catalog.search();}
    private void library(){catalog.library();}
    void notice(String message){status.setText(message);}
    void login(){
        LinearLayout content=new LinearLayout(this);content.setOrientation(LinearLayout.VERTICAL);content.setPadding(dp(20),dp(8),dp(20),0);
        TextView help=label("Use o Client ID do mesmo aplicativo configurado no Tonal para Windows. Cadastre este endereço no painel Spotify:\n"+SpotifyClient.REDIRECT,14,MUTED);help.setTextIsSelectable(true);content.addView(help);
        EditText id=new EditText(this);id.setSingleLine(true);id.setTextColor(TEXT);id.setText(getSharedPreferences("settings",0).getString("clientId",""));id.setHint("Client ID");content.addView(id);
        new AlertDialog.Builder(this).setTitle("Conectar Spotify").setView(content).setNegativeButton("Cancelar",null).setPositiveButton("Entrar",(dialog,which)->{
            PlaybackService.start(this,"authorize",null);
            status.setText("Conclua o login no navegador…");api.login(id.getText().toString().trim(),message->{status.setText(message);if(message.equals("Login concluído")){home();connectSpotify();}else if(!NativeBridge.ready)stopService(new Intent(this,PlaybackService.class));});
        }).show();
    }
    void authorizeTextPlaylist(){String id=getSharedPreferences("settings",0).getString("clientId","");if(id.isEmpty()){login();return;}api.login(id,this::notice);}
    void createPlaylist(){new AlertDialog.Builder(this).setTitle("Criar playlist").setItems(new String[]{"Playlist pessoal","Criar com Text to Tonalify…"},(d,n)->{if(n==1)TextPlaylistWizard.show(this,api);else MusicPanel.show(this,api);}).show();}
    private void connectSpotify(){
        AudioFiles.cancel();
        if(authorizingEngine){status.setText("Conclua a autorização de reprodução no navegador…");return;}
        connecting=true;local=false;NativeBridge.ready=false;NativeBridge.command("stop");NativeBridge.audio.release();status.setText("Preparando conexão…");
        boolean cached=new File(new File(getFilesDir(),SpotifyClient.ENGINE_CACHE),"credentials.json").isFile();
        if(!cached&&!engineApi.loggedIn()){
            authorizingEngine=true;PlaybackService.start(this,"authorize",null);
            status.setText("Autorize também a reprodução de áudio no navegador…");
            engineApi.login(SpotifyClient.ENGINE_CLIENT_ID,message->{authorizingEngine=false;status.setText(message);if(engineApi.loggedIn())connectSpotify();else{connecting=false;pendingPlayback=null;stopService(new Intent(this,PlaybackService.class));}});
            return;
        }
        engineApi.worker.execute(()->{try{String token=cached?"":engineApi.accessToken();runOnUiThread(()->PlaybackService.start(this,"connect",token));}catch(Exception error){runOnUiThread(()->{connecting=false;pendingPlayback=null;status.setText(error.getMessage());stopService(new Intent(this,PlaybackService.class));});}});
    }
    private void clearAudioCredentials(){for(String name:new String[]{"librespot",SpotifyClient.ENGINE_CACHE})new File(new File(getFilesDir(),name),"credentials.json").delete();}
    private void mixUiFixture(){if(!BuildConfig.DEBUG)return;try{MusicStore store=MusicStore.get(this);for(int i=1;i<=6;i++){JSONObject t=new JSONObject();MusicStore.put(t,"provider","Fixture");MusicStore.put(t,"id","mix-fixture-"+i);MusicStore.put(t,"uri","fixture:"+i);MusicStore.put(t,"title","Mix Fixture "+i);MusicStore.put(t,"artist","Test Band");MusicStore.put(t,"album","Test Album");MusicStore.put(t,"genre","grunge");MusicStore.put(t,"year",1994);MusicStore.put(t,"durationMs",180000);store.favorite(t,true);if(i==1)store.select(t);}store.lines=new JSONArray().put(new JSONObject().put("text","Synthetic line for layout verification").put("time",-1));store.lyricStatus="Letra sintética de teste local";
        ArrayList<String> source=new ArrayList<>(),pool=new ArrayList<>();for(int i=0;i<6;i++)Collections.addAll(source,"s"+i,"Source Band","album","grunge","1994","0","0");for(int i=0;i<3;i++)Collections.addAll(pool,"p"+i,"New Band "+i,"other","grunge","1993","0","0");int[] plan=ListeningMixNative.plan(source.toArray(new String[0]),pool.toArray(new String[0]),0,false,true,42,true);JSONArray sequence=new JSONArray(),order=new JSONArray();for(int n:plan)sequence.put(n);for(int n:ListeningMixNative.order(6,42))order.put(n);JSONObject report=new JSONObject().put("native_cpp_via_jni",true).put("sequence",sequence).put("shaken_order",order).put("real_spotify_writes",0);java.nio.file.Files.write(new File(getExternalFilesDir(null),"listening-mix-tests.json").toPath(),report.toString().getBytes(StandardCharsets.UTF_8));orderedQueue.clear();for(int i=1;i<=6;i++){JSONObject t=new JSONObject().put("uri","fixture:"+i).put("name","Mix Fixture "+i).put("artists",new JSONArray().put(new JSONObject().put("name","Test Band"))).put("duration_ms",180000).put("album",new JSONObject().put("name","Test Album").put("release_date","1994"));orderedQueue.add(t);}orderedCursor=0;currentTrack=orderedQueue.get(0);currentUri="fixture:1";DebugLyricChecks.run(this,store,()->MusicPanel.showSong(this,api));
        }catch(Exception e){throw new IllegalStateException("Native mix fixture failed",e);}}
    View toolbar(Runnable leave,Runnable favorites){
        LinearLayout header=ui.row();ui.pad(header,12,4);ImageView logo=new ImageView(this);logo.setImageDrawable(new Ui.Symbol("logo",Ui.BLUE));header.addView(logo,new LinearLayout.LayoutParams(dp(30),dp(30)));TextView brand=ui.title("  Tonalify",20);brand.setSingleLine();brand.setContentDescription("Tonal Hub");ui.clickable(brand,()->{leave.run();home();});ui.weight(header,brand);
        header.addView(ui.icon("search","Buscar no Spotify",()->{leave.run();search();}),new LinearLayout.LayoutParams(dp(44),dp(44)));
        header.addView(ui.icon("heart","Biblioteca pessoal, estatísticas e letras",favorites),new LinearLayout.LayoutParams(dp(44),dp(44)));
        header.addView(ui.icon("audio","Áudio",this::audioSettings),new LinearLayout.LayoutParams(dp(44),dp(44)));
        header.addView(ui.icon("account","Conta",this::account),new LinearLayout.LayoutParams(dp(44),dp(44)));return header;
    }
    private void buildShell(){
        root=ui.column();root.setBackgroundColor(Ui.BG);
        Ui.insets(root);
        root.addView(toolbar(()->{},()->MusicPanel.show(this,api)));
        status=ui.text(NativeBridge.status,11,Ui.MUTED);status.setId(R.id.playback_status);status.setMaxLines(2);ui.pad(status,16,2);root.addView(status);
        body=ui.column();root.addView(body,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout transport=ui.column();ui.pad(transport,10,4);LinearLayout mini=ui.row();ui.pad(mini,8,5);ui.rounded(mini,Ui.CARD,8);
        nowCover=new ImageView(this);nowCover.setScaleType(ImageView.ScaleType.CENTER_CROP);ui.rounded(nowCover,Ui.PANEL,4);nowCover.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));mini.addView(nowCover,new LinearLayout.LayoutParams(dp(44),dp(44)));
        LinearLayout labels=ui.column();ui.pad(labels,10,0);now=ui.line("Escolha sua música",14,Ui.TEXT);now.setId(R.id.now_title);now.setTypeface(null,Typeface.BOLD);byline=ui.line("Spotify e arquivos locais",12,Ui.MUTED);byline.setId(R.id.now_artist);labels.addView(now);ui.space(labels,4);labels.addView(byline);ui.clickable(labels,this::player);ui.weight(mini,labels);
        favoriteButton=ui.icon("heart","Favoritar música",()->MusicPanel.toggleFavorite(this,api));mini.addView(favoriteButton,new LinearLayout.LayoutParams(dp(44),dp(44)));ui.clickable(nowCover,this::player);
        toggle=ui.icon("play","Reproduzir",()->{if(NativeBridge.ready||local)NativeBridge.command("toggle");else if(currentTrack!=null)playTracks(new ArrayList<>(List.of(currentTrack)),0,"",currentTrack);else notice("Escolha uma música na busca ou na biblioteca.");});toggle.setId(R.id.play_pause);mini.addView(toggle,new LinearLayout.LayoutParams(dp(44),dp(44)));
        mini.addView(ui.icon("next","Próxima",()->NativeBridge.command("next")),new LinearLayout.LayoutParams(dp(40),dp(44)));transport.addView(mini);
        progress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);progress.setProgressTintList(android.content.res.ColorStateList.valueOf(Ui.BLUE));progress.setMax(1000);progress.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);transport.addView(progress,new LinearLayout.LayoutParams(-1,dp(3)));
        time=ui.text("",10,Ui.MUTED);time.setId(R.id.pcm_metrics);time.setVisibility(diagnostics?View.VISIBLE:View.GONE);transport.addView(time);root.addView(transport);
        LinearLayout nav=ui.row();String[] names={"Início","Buscar","Biblioteca"},icons={"home","search","library"};Runnable[] actions={this::home,this::search,this::library};
        for(int i=0;i<3;i++){LinearLayout cell=ui.column();cell.setGravity(Gravity.CENTER);ImageView icon=new ImageView(this);icon.setImageDrawable(new Ui.Symbol(icons[i],Ui.MUTED));cell.addView(icon,new LinearLayout.LayoutParams(dp(22),dp(22)));TextView label=ui.text(names[i],11,Ui.MUTED);label.setGravity(Gravity.CENTER);ui.pad(label,0,5);cell.addView(label);tabs.add(label);final Runnable action=actions[i];ui.clickable(cell,action);nav.addView(cell,new LinearLayout.LayoutParams(0,dp(60),1));}root.addView(nav);
    }
    void selectTab(String page){int active=page.equals("search")?1:page.equals("library")?2:0;for(int i=0;i<tabs.size();i++)tabs.get(i).setTextColor(i==active?Ui.TEXT:Ui.MUTED);}
    private void account(){
        String[] names=api.loggedIn()?new String[]{"Atualizar autorização da biblioteca","Conectar motor Spotify","Áudio","Abrir arquivo de áudio","Testar áudio por 60 segundos","Refazer autorização do áudio","Parar motor de áudio","Sair do Spotify"}:new String[]{"Entrar com Spotify","Áudio","Abrir arquivo de áudio","Testar áudio por 60 segundos"};
        new AlertDialog.Builder(this).setTitle("Tonalify • "+BuildConfig.VERSION_NAME).setItems(names,(dialog,index)->{switch(names[index]){
            case "Entrar com Spotify":case "Atualizar autorização da biblioteca":login();break;
            case "Conectar motor Spotify":connectSpotify();break;case "Áudio":audioSettings();break;case "Abrir arquivo de áudio":openFile();break;
            case "Testar áudio por 60 segundos":api.worker.execute(()->{try{File test=copyAsset();runOnUiThread(()->playLocal(test,"Ensaio de áudio Vorbis"));}catch(IOException e){runOnUiThread(()->notice(e.getMessage()));}});break;
            case "Refazer autorização do áudio":stopService(new Intent(this,PlaybackService.class));engineApi.logout();clearAudioCredentials();connectSpotify();break;
            case "Parar motor de áudio":pendingPlayback=null;connecting=false;stopService(new Intent(this,PlaybackService.class));notice("Motor parado");break;
            case "Sair do Spotify":pendingPlayback=null;connecting=false;stopService(new Intent(this,PlaybackService.class));api.logout();engineApi.logout();clearAudioCredentials();local=false;currentTrack=null;currentUri="";now.setText("Escolha sua música");byline.setText("Spotify e arquivos locais");nowCover.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));home();break;
        }}).setNegativeButton("Fechar",null).show();
    }
    final ArrayList<JSONObject> orderedQueue=new ArrayList<>();int orderedCursor;
    void cancelMix(){if(listeningMix!=null)listeningMix.cancel();}
    void playMix(ArrayList<JSONObject> tracks,int selected,boolean random,boolean discovery,boolean album){if(listeningMix==null)listeningMix=new ListeningMix(api,MusicStore.get(this));listeningMix.prepare(tracks,selected,random,discovery,album,result->{if(!destroyed&&foreground&&!result.isEmpty()){NativeBridge.command("shuffle:off");playTracks(result,0,"",result.get(0));}},this::notice);}
    void playTracks(ArrayList<JSONObject> tracks,int index,String context,JSONObject selected){playTracks(tracks,index,context,selected,0);}
    private void playTracks(ArrayList<JSONObject> tracks,int index,String context,JSONObject selected,int resumePosition){
        if(!foreground)return;
        cancelMix();
        orderedQueue.clear();orderedCursor=0;
        if(!api.loggedIn()){login();return;}if(index<0||index>=tracks.size())return;
        if(!selected.optString("uri").startsWith("spotify:track:")||selected.optBoolean("is_local")||(selected.has("is_playable")&&!selected.optBoolean("is_playable"))){notice("Esta faixa não está disponível para streaming nesta conta.");return;}
        for(JSONObject metadata:tracks)MusicStore.get(this).remember(MusicStore.spotifyTrack(metadata));
        ArrayList<String> uris=new ArrayList<>();int selectedIndex=0;for(int i=Math.max(0,index-250);i<tracks.size()&&uris.size()<500;i++){String uri=tracks.get(i).optString("uri");JSONObject track=tracks.get(i);if(!uri.startsWith("spotify:track:")||track.optBoolean("is_local")||(track.has("is_playable")&&!track.optBoolean("is_playable")))continue;if(i==index)selectedIndex=uris.size();uris.add(uri);}if(uris.isEmpty())return;
        if(context.isEmpty()){for(String uri:uris)for(JSONObject item:tracks)if(uri.equals(item.optString("uri"))){orderedQueue.add(item);break;}orderedCursor=selectedIndex;}
        String command=context.isEmpty()?"queue|"+selectedIndex+"|"+String.join(",",uris):"context|"+selected.optInt("_contextIndex",index)+"|"+context;
        if(resumePosition>0)command="restore|"+resumePosition+"|"+selected.optString("uri")+"|"+command;
        final String selectionCommand=command;
        AudioFiles.cancel();playbackRevision++;expectedUri=selected.optString("uri");expectedUntil=SystemClock.elapsedRealtime()+15000;local=false;clockPlaying=false;setTrack(selected);positionMs=resumePosition;positionAt=SystemClock.elapsedRealtime();pendingPlayback=()->{NativeBridge.command(selectionCommand);lastPoll=0;};
        if(NativeBridge.ready){Runnable run=pendingPlayback;pendingPlayback=null;run.run();}else if(!connecting)connectSpotify();
    }
    void showQueue(){new MixQueueDialog(this,api,MusicStore.get(this)).show();}
    void queueSnapshot(Consumer<ArrayList<JSONObject>> ready,Consumer<String> error){
        if(!orderedQueue.isEmpty()){ready.accept(new ArrayList<>(orderedQueue.subList(Math.min(orderedCursor,orderedQueue.size()),orderedQueue.size())));return;}
        api.get("me/player/queue",r->{ArrayList<JSONObject> tracks=new ArrayList<>();JSONObject current=r.optJSONObject("currently_playing");if(current!=null)tracks.add(current);else if(currentTrack!=null)tracks.add(currentTrack);for(Object v:MusicStore.values(MusicStore.array(r,"queue")))if(v instanceof JSONObject)tracks.add((JSONObject)v);ready.accept(tracks);},error);
    }
    void applyPreparedQueue(ArrayList<JSONObject> queue,String anchor){if(queue.isEmpty()||!anchor.equals(currentUri)){notice("A música mudou. Prepare a fila novamente.");return;}int position=currentPosition();NativeBridge.command("shuffle:off");playTracks(queue,0,"",queue.get(0),position);}
    void searchArtist(String query){catalog.search(query);}
    void playPersonal(JSONArray values){playPersonalMix(values,false,false);}
    void playPersonalMix(JSONArray values,boolean random,boolean discovery){ArrayList<JSONObject> tracks=new ArrayList<>();for(Object v:MusicStore.values(values)){JSONObject t=(JSONObject)v;if(t.optString("provider").equals("Local")){if(tracks.isEmpty()){playLocal(new File(t.optString("localFile")),t.optString("title"));return;}continue;}if(!t.optString("provider").equals("Spotify"))continue;JSONObject item=new JSONObject();MusicStore.put(item,"uri",t.optString("uri"));MusicStore.put(item,"name",t.optString("title"));MusicStore.put(item,"duration_ms",t.optInt("durationMs"));JSONObject artist=new JSONObject();MusicStore.put(artist,"name",t.optString("artist"));MusicStore.put(artist,"id",t.optString("artistId"));MusicStore.put(item,"artists",new JSONArray().put(artist));JSONObject album=new JSONObject();MusicStore.put(album,"name",t.optString("album"));MusicStore.put(album,"id",t.optString("albumId"));if(t.optInt("year")>0)MusicStore.put(album,"release_date",String.valueOf(t.optInt("year")));JSONObject img=new JSONObject();MusicStore.put(img,"url",t.optString("image"));MusicStore.put(album,"images",new JSONArray().put(img));MusicStore.put(item,"album",album);tracks.add(item);}if(!tracks.isEmpty()){if(random||discovery)playMix(tracks,0,random,discovery,false);else playTracks(tracks,0,"",tracks.get(0));}}
    void trackPersonalMenu(JSONObject track){MusicPanel.trackMenu(this,api,track);}
    private void setTrack(JSONObject item){if(!item.optString("uri").equals(currentUri)&&!orderedQueue.isEmpty()){int found=-1;for(int i=orderedCursor;i<orderedQueue.size();i++)if(item.optString("uri").equals(orderedQueue.get(i).optString("uri"))){found=i;break;}if(found<0){orderedQueue.clear();orderedCursor=0;}else orderedCursor=found;}MusicStore journal=MusicStore.get(this);journal.select(MusicStore.spotifyTrack(item));if(musicMetadata==null){if(journal.metadata!=null)journal.metadata.close();musicMetadata=new MusicMetadata(journal,api);journal.metadata=musicMetadata;}musicMetadata.enrich(journal.current);currentTrack=item;currentUri=item.optString("uri");if(!currentUri.isEmpty()&&!currentUri.equals(queriedLikeUri)){queriedLikeUri=currentUri;String requested=currentUri;api.get("me/library/contains?uris="+SpotifyClient.encode(requested),r->{JSONArray items=r.optJSONArray("items");if(requested.equals(currentUri)&&items!=null&&items.length()>0)MusicStore.get(this).favorite(MusicStore.spotifyTrack(item),items.optBoolean(0));},error->{});}now.setText(item.optString("name"));byline.setText(CatalogScreen.artists(item));durationMs=item.optInt("duration_ms");artwork.load(nowCover,journal.current.optString("image"));}
    private void updatePlayer(JSONObject json){if(local||!NativeBridge.ready)return;JSONObject device=json.optJSONObject("device");if(engineDeviceId.isEmpty())try{engineDeviceId=new String(java.nio.file.Files.readAllBytes(new File(new File(getFilesDir(),SpotifyClient.ENGINE_CACHE),"device-id").toPath()),StandardCharsets.UTF_8).trim();}catch(IOException ignored){}if(device==null||engineDeviceId.isEmpty()||!engineDeviceId.equals(device.optString("id")))return;JSONObject item=json.optJSONObject("item");if(item!=null){if(!expectedUri.isEmpty()){if(!expectedUri.equals(item.optString("uri"))&&SystemClock.elapsedRealtime()<expectedUntil)return;expectedUri="";}if(!item.optString("uri").equals(currentUri)||currentTrack==null||Artwork.url(currentTrack).isEmpty())setTrack(item);positionMs=json.optInt("progress_ms");positionAt=SystemClock.elapsedRealtime();clockPlaying=NativeBridge.playing&&json.optBoolean("is_playing");durationMs=item.optInt("duration_ms");}}
    private static void changedText(TextView view,String value){if(!value.contentEquals(view.getText()))view.setText(value);}
    int currentPosition(){return (int)Math.min(durationMs,positionMs+(clockPlaying?SystemClock.elapsedRealtime()-positionAt:0));}
    private void updateTransport(){if(favoriteButton!=null){boolean liked=MusicStore.get(this).liked(MusicStore.get(this).current);String label=liked?"Remover das favoritas":"Favoritar música";if(!label.contentEquals(favoriteButton.getContentDescription())){favoriteButton.setContentDescription(label);favoriteButton.setImageDrawable(new Ui.Symbol(liked?"heart_filled":"heart",liked?Ui.BLUE:Ui.TEXT));}}if(toggle!=null){String description=NativeBridge.playing?"Pausar":"Reproduzir";if(!description.contentEquals(toggle.getContentDescription())){toggle.setContentDescription(description);toggle.setImageDrawable(new Ui.Symbol(NativeBridge.playing?"pause":"play",Ui.TEXT));}}if(progress!=null)progress.setProgress(durationMs>0?(int)(currentPosition()*1000L/durationMs):0);if(time!=null&&diagnostics){long frames=NativeBridge.audio.frames.get(),blocks=NativeBridge.audio.blocks.get();double micros=blocks==0?0:NativeBridge.audio.dspNanos.get()/1000.0/blocks;changedText(time,String.format(java.util.Locale.ROOT,"PCM: %,d frames · DSP %.0f µs/bloco · underruns %d",frames,micros,NativeBridge.audio.underruns()));}}
    private void player(){MusicPanel.showSong(this,api);}
    private ScrollView playerScroll(View panel){ScrollView scroll=new ScrollView(this);scroll.addView(panel);return scroll;}
    // API 33+ uses the registered OnBackInvokedCallback; this fallback is for API 28–32.
    @android.annotation.SuppressLint("GestureBackNavigation")
    @Override public void onBackPressed(){if(Build.VERSION.SDK_INT<33&&(catalog==null||!catalog.back()))super.onBackPressed();}
    private void openFile(){Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").putExtra(Intent.EXTRA_MIME_TYPES,new String[]{"audio/*","application/octet-stream"}).addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,20);}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);TextPlaylistWizard.photoResult(this,request,result,data);MusicPanel.photoResult(this,request,result,data);MusicPanel.backupResult(this,request,result,data);if(request==20&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri uri=data.getData();api.worker.execute(()->{try{
        File file=new File(getCacheDir(),"audio-open-"+System.nanoTime());try(InputStream input=getContentResolver().openInputStream(uri);OutputStream output=new FileOutputStream(file)){if(input==null)throw new IOException("Arquivo inacessível");byte[] bytes=new byte[65536];int n;long total=0;while((n=input.read(bytes))!=-1){total+=n;if(total>512L*1024*1024)throw new IOException("Limite desta prévia: 512 MB por arquivo");output.write(bytes,0,n);}}
        runOnUiThread(()->playLocal(file,"Arquivo local"));
    }catch(Exception error){runOnUiThread(()->status.setText(error.getMessage()));}});}}
    private File copyAsset() throws IOException {File file=new File(getCacheDir(),"benchmark.ogg");if(!file.exists())try(InputStream input=getAssets().open("benchmark.ogg");OutputStream output=new FileOutputStream(file)){byte[] bytes=new byte[65536];int n;while((n=input.read(bytes))!=-1)output.write(bytes,0,n);}return file;}
    private void playLocal(File file,String name){
        NativeBridge.playing=false;NativeBridge.ready=false;clockPlaying=false;positionMs=0;
        playbackRevision++;expectedUri="";local=true;NativeBridge.command("stop");NativeBridge.audio.release();pendingPlayback=null;connecting=false;currentTrack=null;currentUri="";durationMs=0;now.setText(name);byline.setText("Preparando áudio local…");nowCover.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));
        JSONObject localTrack=new JSONObject();MusicStore.put(localTrack,"provider","Local");MusicStore.put(localTrack,"id",file.getAbsolutePath());MusicStore.put(localTrack,"title",name);MusicStore.put(localTrack,"localFile",file.getAbsolutePath());MusicStore.put(localTrack,"playable",true);MusicStore.get(this).select(localTrack);
        long generation=AudioFiles.cancel();NativeBridge.onStatus("Decodificando e analisando volume do arquivo…");
        api.worker.execute(()->{try{AudioFiles.Prepared prepared=AudioFiles.prepare(this,file,generation);runOnUiThread(()->{if(destroyed||!foreground||AudioFiles.serial.get()!=generation){prepared.file.delete();return;}prepared.activate();byline.setText(prepared.info.optBoolean("binaural")?"IAMF binaural • libiamf/OAR":"Arquivo local • FFmpeg");PlaybackService.start(this,"file",prepared.file.getAbsolutePath());});}catch(Exception error){if(AudioFiles.serial.get()==generation)runOnUiThread(()->NativeBridge.onStatus("Falha ao preparar áudio: "+error.getMessage()));}});
    }
    private void loadDsp(){NativeBridge.audio.initialize(this);}
    private void audioSettings(){AudioPanel.show(this);}
    void reconnectAudio(){AudioFiles.cancel();synchronized(NativeBridge.intent){PlaybackIntent saved=NativeBridge.intent;if(saved.desired&&!saved.resumeSelection().isEmpty()){String command=saved.restoreCommand();pendingPlayback=()->NativeBridge.command(command);}}connectSpotify();}
    @Override protected void onNewIntent(Intent intent){
        super.onNewIntent(intent);
        String literal=intent.getStringExtra("literalQuery");if(literal!=null)catalog.search(literal);
        if(!BuildConfig.DEBUG)return;
        try{
            if(intent.getBooleanExtra("audioTestSettings",false)){
                AudioOutput audio=NativeBridge.audio;AudioConfig c=AudioConfig.load(audio.prefs(),audio.routeKey);
                c.preset(Math.max(0,Math.min(5,intent.getIntExtra("audioPreset",0))));c.spatial=intent.getBooleanExtra("audioHrtf",false)?1:0;c.profile=c.spatial==1?1:0;c.buffer=Math.max(0,Math.min(2,intent.getIntExtra("audioBuffer",0)));c.enabled=!intent.getBooleanExtra("audioBypass",false);audio.updateConfig(c,audio.routeKey);
            }
            if(intent.getBooleanExtra("audioRecover",false))NativeBridge.audio.testDeadObject();
            if(intent.hasExtra("audioCommand"))NativeBridge.command(intent.getStringExtra("audioCommand"));
            if(intent.getBooleanExtra("audioSnapshot",false))java.nio.file.Files.write(new File(getFilesDir(),"audio-snapshot.json").toPath(),NativeBridge.audio.testSnapshot().put("engineStatus",NativeBridge.status).put("currentUri",currentUri).put("positionMs",currentPosition()).toString().getBytes(StandardCharsets.UTF_8));
        }catch(Exception error){android.util.Log.e("TonalTest","Audio test hook failed",error);}
    }
    @Override protected void onStart(){super.onStart();foreground=true;handler.removeCallbacks(ticker);handler.post(ticker);}
    @Override protected void onStop(){
        foreground=false;cancelMix();
        handler.removeCallbacks(ticker);
        // Do not start a pending selection after the user locks or leaves.
        if(pendingPlayback!=null){pendingPlayback=null;expectedUri="";NativeBridge.command("pause");}
        super.onStop();
    }
    @Override protected void onDestroy(){destroyed=true;cancelMix();MusicStore journal=MusicStore.get(this);if(journal.metadata!=null&&journal.metadata.uses(api)){journal.metadata.close();journal.metadata=null;}musicMetadata=null;NativeBridge.listeners.remove(statusListener);NativeBridge.commandListeners.remove(commandObserver);handler.removeCallbacks(ticker);catalog.close();artwork.close();api.close();engineApi.close();super.onDestroy();}
}
