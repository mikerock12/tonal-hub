package app.tonal.android;

import android.app.*;
import android.os.*;
import android.view.View;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Native setlist.fm search -> the existing shared-C++ review/import wizard.
 * No Spotify write is performed by searching or selecting shows. */
final class SetlistWizard {
    private final MainActivity activity;private final SpotifyClient api;private final Ui ui;private final Secrets secrets;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();private final Handler main=new Handler(Looper.getMainLooper());
    private Dialog dialog;private LinearLayout results;private EditText keyInput,artistInput;private TextView status,search,more,importButton;
    private boolean closed,busy;private int generation,page=1;private String artist="",key="";
    private final LinkedHashMap<String,JSONObject> selected=new LinkedHashMap<>();
    static void show(MainActivity a,SpotifyClient api){new SetlistWizard(a,api).show();}
    private SetlistWizard(MainActivity a,SpotifyClient client){activity=a;api=client;ui=new Ui(a);secrets=new Secrets(a,"setlistfm");JSONObject saved=secrets.load();if(saved!=null)key=saved.optString("key");}
    private void show(){
        dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,16,10);Ui.insets(root);
        LinearLayout heading=ui.row();ui.weight(heading,ui.title("Playlist de shows",24));heading.addView(ui.icon("close","Fechar setlist.fm",dialog::dismiss));root.addView(heading);
        root.addView(ui.text("Escolha shows no setlist.fm e revise as gravações encontradas no Spotify. O áudio será das versões escolhidas, não necessariamente do show.",14,Ui.MUTED));
        keyInput=new EditText(activity);keyInput.setTextColor(Ui.TEXT);keyInput.setHintTextColor(Ui.MUTED);keyInput.setHint("Sua API key setlist.fm");keyInput.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);keyInput.setSingleLine();keyInput.setText(key);root.addView(keyInput);
        LinearLayout keyActions=ui.row();ui.weight(keyActions,ui.action("Obter chave gratuita",()->activity.startActivity(new android.content.Intent(android.content.Intent.ACTION_VIEW,android.net.Uri.parse("https://www.setlist.fm/settings/api")))));ui.weight(keyActions,ui.action("Esquecer chave",()->{++generation;busy=false;search.setEnabled(true);more.setEnabled(true);secrets.clear();key="";keyInput.setText("");}));root.addView(keyActions);
        artistInput=new EditText(activity);artistInput.setTextColor(Ui.TEXT);artistInput.setHintTextColor(Ui.MUTED);artistInput.setHint("Banda / artista");artistInput.setSingleLine();root.addView(artistInput);
        search=ui.action("Buscar shows",()->load(true));root.addView(search);status=ui.text("Dados: setlist.fm · chave guardada com Android Keystore",13,Ui.MUTED);root.addView(status);
        results=ui.column();ScrollView scroll=new ScrollView(activity);scroll.addView(results);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        more=ui.action("Mais shows",()->load(false));more.setVisibility(View.GONE);root.addView(more);
        importButton=ui.action("Revisar músicas dos shows selecionados",this::importShows);root.addView(importButton);importButton.setEnabled(false);
        dialog.setContentView(root);dialog.setOnDismissListener(d->{closed=true;++generation;worker.shutdownNow();main.removeCallbacksAndMessages(null);});dialog.show();dialog.getWindow().setLayout(-1,-1);
    }
    private void load(boolean reset){
        if(busy)return;String name=artistInput.getText().toString().trim(),secret=keyInput.getText().toString().trim();if(secret.isEmpty()||name.isEmpty()){status.setText("Informe artista e chave setlist.fm.");return;}
        if(reset){page=1;artist=name;key=secret;selected.clear();results.removeAllViews();importButton.setEnabled(false);}else if(!artist.equals(name)||!key.equals(secret)){status.setText("Clique em Buscar shows para iniciar a nova pesquisa.");return;}
        busy=true;search.setEnabled(false);more.setEnabled(false);status.setText("Buscando shows…");int request=++generation,requestedPage=page;String requestedArtist=artist,requestedKey=key;
        worker.execute(()->{JSONObject response=null;String error="";try{response=request(requestedArtist,requestedPage,requestedKey);}catch(WebRequest.Failure e){error=e.code==401||e.code==403?"Chave recusada pelo setlist.fm.":e.code==429?"Limite do setlist.fm. Aguarde antes de buscar novamente.":"setlist.fm indisponível (HTTP "+e.code+"). Tente mais tarde.";}catch(Exception e){error="Não foi possível consultar o setlist.fm. Confira a conexão.";}
            JSONObject data=response;String failure=error;main.post(()->{if(closed||request!=generation)return;busy=false;search.setEnabled(true);more.setEnabled(true);if(!failure.isEmpty()){status.setText(failure);return;}try{JSONObject saved=new JSONObject();saved.put("key",requestedKey);secrets.save(saved);}catch(Exception e){status.setText("Não foi possível guardar a chave com segurança.");return;}JSONArray shows=flatten(data);for(Object value:MusicStore.values(shows))showRow((JSONObject)value);int total=data.optInt("total");page++;more.setVisibility(requestedPage<5&&requestedPage*data.optInt("itemsPerPage",20)<total?View.VISIBLE:View.GONE);status.setText(shows.length()==0?"Nenhum show com músicas encontrado nesta página.":"Selecione shows e confira suas músicas antes de importar. · setlist.fm");});
        });
    }
    private static JSONObject request(String artist,int page,String key)throws Exception{
        HttpURLConnection c=(HttpURLConnection)new URL(WebRequest.params("https://api.setlist.fm/rest/1.0/search/setlists","artistName",artist,"p",Integer.toString(page))).openConnection();
        c.setInstanceFollowRedirects(false);c.setConnectTimeout(10000);c.setReadTimeout(10000);c.setRequestProperty("x-api-key",key);c.setRequestProperty("Accept","application/json");c.setRequestProperty("Accept-Language","pt");c.setRequestProperty("User-Agent","TonalHub/0.14.0 (https://github.com/mikerock12/tonal-hub)");
        ScheduledFuture<?> timeout=WebRequest.deadlines.schedule(c::disconnect,18,TimeUnit.SECONDS);
        try{int code=c.getResponseCode();if(code==404)return new JSONObject();if(code!=200)throw new WebRequest.Failure(code,"");try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[8192];for(int n;(n=in.read(b))!=-1;){if(Thread.currentThread().isInterrupted())throw new InterruptedIOException();if(out.size()+n>2*1024*1024)throw new IOException();out.write(b,0,n);}return new JSONObject(new String(out.toByteArray(),StandardCharsets.UTF_8));}}finally{timeout.cancel(false);c.disconnect();}
    }
    static JSONArray flatten(JSONObject response){JSONArray output=new JSONArray();for(Object v:MusicStore.values(MusicStore.array(response,"setlist"))){JSONObject s=(JSONObject)v;JSONArray songs=new JSONArray();String artist=MusicStore.object(s,"artist").optString("name");sets:for(Object set:MusicStore.values(MusicStore.array(MusicStore.object(s,"sets"),"set")))for(Object value:MusicStore.values(MusicStore.array((JSONObject)set,"song"))){JSONObject song=(JSONObject)value;String name=song.optString("name").trim();if(song.optBoolean("tape")||name.isEmpty())continue;songs.put(artist+" — "+name);if(songs.length()>=500)break sets;}if(songs.length()==0)continue;JSONObject show=new JSONObject();for(String k:new String[]{"id","eventDate","url"})MusicStore.put(show,k,s.optString(k));MusicStore.put(show,"artist",artist);JSONObject venue=MusicStore.object(s,"venue");MusicStore.put(show,"place",venue.optString("name")+" · "+MusicStore.object(venue,"city").optString("name"));MusicStore.put(show,"songs",songs);output.put(show);}return output;}
    private void showRow(JSONObject show){LinearLayout row=ui.column();ui.pad(row,12,12);ui.rounded(row,Ui.CARD,14);CheckBox check=new CheckBox(activity);String label=show.optString("artist")+" · "+show.optString("eventDate");check.setText(label+"\n"+show.optString("place"));check.setTextColor(Ui.TEXT);row.addView(check);TextView songs=ui.text(String.join("\n",strings(MusicStore.array(show,"songs"))),14,Ui.MUTED);songs.setVisibility(View.GONE);row.addView(ui.action("Ver / ocultar "+MusicStore.array(show,"songs").length()+" músicas",()->songs.setVisibility(songs.getVisibility()==View.GONE?View.VISIBLE:View.GONE)));row.addView(songs);check.setOnCheckedChangeListener((v,on)->{if(on)selected.put(show.optString("id"),show);else selected.remove(show.optString("id"));importButton.setEnabled(!selected.isEmpty());});results.addView(row);ui.space(results,12);}
    private static ArrayList<String> strings(JSONArray array){ArrayList<String> out=new ArrayList<>();for(int i=0;i<array.length();i++)out.add(array.optString(i));return out;}
    private void importShows(){if(selected.isEmpty())return;StringBuilder text=new StringBuilder(),description=new StringBuilder("Seleção de shows · setlist.fm\n");int count=0;for(JSONObject show:selected.values()){description.append(show.optString("url")).append('\n');for(String song:strings(MusicStore.array(show,"songs"))){if(++count>500){status.setText("Selecione até 500 músicas por importação.");return;}text.append(song).append('\n');}}if(text.length()>128*1024){status.setText("O texto ultrapassa 128 KiB. Selecione menos shows.");return;}String name=artist+" · "+(selected.size()==1?selected.values().iterator().next().optString("eventDate"):"shows");dialog.dismiss();TextPlaylistWizard.fromSetlist(activity,api,text.toString(),name,description.toString());}
}
