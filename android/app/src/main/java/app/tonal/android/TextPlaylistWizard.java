package app.tonal.android;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.net.Uri;
import android.os.*;
import android.text.InputFilter;
import android.util.Base64;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.lang.ref.WeakReference;
import java.util.*;

/** Native views and existing Spotify transport; business rules execute in C++. */
final class TextPlaylistWizard {
    static final int COVER_REQUEST=184;
    private static WeakReference<TextPlaylistWizard> pendingCover=new WeakReference<>(null);
    private final MainActivity activity;private final SpotifyClient api;private final MusicStore store;private final Ui ui;
    private final Handler main=new Handler(Looper.getMainLooper());private Dialog dialog;private LinearLayout body,footer;
    private TextView notice;private ProgressBar progress;private EditText sourceInput,artistInput,nameInput,descriptionInput;private Spinner formatInput,visibilityInput;private CheckBox deduplicateInput;
    private String raw="",defaultArtist="",name="Minha playlist de texto",description="",cover="",saved="",cloudId="",message="Cole uma lista, setlist ou músicas de um show.";
    private int format,step,page,generation,cursor,verifyPages;private boolean busy,syncing,closed,deduplicate;
    private final ArrayList<Row> rows=new ArrayList<>();private JSONArray upload=new JSONArray();
    private final boolean fixture;
    private int fixtureCreates,fixtureWrites;private boolean fixtureDropped,fixturePublic;private String fixtureMime="";private final ArrayList<String> fixtureRemote=new ArrayList<>();
    private static final class Row {String artist,title,original,status="Aguardando";JSONArray candidates=new JSONArray();int choice=-1;boolean selected;}
    static void show(MainActivity a,SpotifyClient api){new TextPlaylistWizard(a,api).show();}
    static void resume(MainActivity a,SpotifyClient api,String id){TextPlaylistWizard w=new TextPlaylistWizard(a,api);JSONObject p=w.store.playlist(id);if(p.length()==0)return;w.saved=id;w.cover=p.optString("cover");w.name=p.optString("name");w.description=p.optString("description");w.step=3;w.message="Playlist salva neste aparelho. Você pode concluir a cópia para o Spotify.";w.show();}
    private TextPlaylistWizard(MainActivity a,SpotifyClient client){activity=a;api=client;store=MusicStore.get(a);ui=new Ui(a);fixture=BuildConfig.DEBUG&&a.getIntent().getBooleanExtra("textPlaylistFixture",false);if(fixture){raw="Veruca Salt - Seether\nL7 - Shove\nNirvana - About a Girl";Bitmap b=Bitmap.createBitmap(48,48,Bitmap.Config.ARGB_8888);b.eraseColor(Ui.BLUE);ByteArrayOutputStream out=new ByteArrayOutputStream();b.compress(Bitmap.CompressFormat.JPEG,80,out);b.recycle();cover="data:image/jpeg;base64,"+Base64.encodeToString(out.toByteArray(),Base64.NO_WRAP);}}
    private void show(){
        dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,16,10);Ui.insets(root);
        LinearLayout header=ui.row();ui.weight(header,ui.title("Text to Tonalify",24));header.addView(ui.icon("close","Fechar",()->{if(!syncing)dialog.dismiss();}));root.addView(header);
        notice=ui.text(message,14,Ui.MUTED);ui.pad(notice,0,12);root.addView(notice);progress=new ProgressBar(activity,null,android.R.attr.progressBarStyleHorizontal);progress.setIndeterminate(true);root.addView(progress,new LinearLayout.LayoutParams(-1,ui.dp(4)));
        body=ui.column();root.addView(body,new LinearLayout.LayoutParams(-1,0,1));footer=ui.row();root.addView(footer);dialog.setContentView(root);
        dialog.setOnDismissListener(d->{closed=true;++generation;main.removeCallbacksAndMessages(null);store.flush();});dialog.show();dialog.getWindow().setLayout(-1,-1);dialog.getWindow().setStatusBarColor(Ui.BG);dialog.getWindow().setNavigationBarColor(Ui.BG);render();
    }
    private void state(String text){message=text;if(notice!=null)notice.setText(text);if(progress!=null)progress.setVisibility(busy?android.view.View.VISIBLE:android.view.View.GONE);if(dialog!=null)dialog.setCancelable(!syncing);}
    private Spinner spinner(String[] values,int selected){Spinner s=new Spinner(activity);s.setAdapter(new ArrayAdapter<>(activity,android.R.layout.simple_spinner_dropdown_item,values));s.setSelection(selected);return s;}
    private EditText field(String hint,String value,int max,boolean multiline){EditText f=new EditText(activity);f.setHint(hint);f.setHintTextColor(Ui.MUTED);f.setTextColor(Ui.TEXT);f.setText(value);f.setTextSize(16);f.setSingleLine(!multiline);f.setFilters(new InputFilter[]{new InputFilter.LengthFilter(max)});return f;}
    private void scroll(LinearLayout content){ScrollView s=new ScrollView(activity);s.addView(content);body.addView(s,new LinearLayout.LayoutParams(-1,-1));}
    private TextView action(String label,Runnable run){TextView b=ui.action(label,run);b.setEnabled(!busy);return b;}
    private void render(){if(closed)return;body.removeAllViews();footer.removeAllViews();state(message);
        if(step==0)paste();else if(step==1)review();else if(step==2)details();else complete();
        if(step>0&&step<3)ui.weight(footer,action("Voltar",()->{collectDetails();--step;render();}));
        if(step==0)ui.weight(footer,action("Interpretar e buscar",this::parse));
        else if(step==1){TextView next=action("Personalizar · "+selectedCount(),()->{step=2;render();});next.setEnabled(!busy&&selectedCount()>0);ui.weight(footer,next);}
        else if(step==2)ui.weight(footer,action("Salvar no Tonalify",this::saveLocal));
        else ui.weight(footer,action("Concluir",()->dialog.dismiss()));
    }
    private void paste(){
        LinearLayout content=ui.column();content.addView(ui.title("Uma lista, muitas possibilidades",23));content.addView(ui.text("Cole músicas ou timestamps. O artista global pode ser detectado automaticamente.",14,Ui.MUTED));
        formatInput=spinner(new String[]{"Automático / Artista — Música","Artista — Música","Música — Artista"},format);content.addView(formatInput);artistInput=field("Artista padrão (opcional)",defaultArtist,300,false);content.addView(artistInput);
        sourceInput=field("Nirvana - MTV Unplugged (Full Concert)\n0:00 About a Girl\n3:39 Come as You Are\n\nVeruca Salt - Seether\nL7 - Shove",raw,128*1024,true);sourceInput.setMinLines(10);sourceInput.setGravity(android.view.Gravity.TOP);content.addView(sourceInput);
        content.addView(ui.text("Até 500 músicas. Nesta etapa nada é criado na conta Spotify.",12,Ui.MUTED));scroll(content);
    }
    private void parse(){
        raw=sourceInput.getText().toString();defaultArtist=artistInput.getText().toString().trim();format=formatInput.getSelectedItemPosition();
        try{String[] parsed=TextPlaylistNative.parse(TextPlaylistNative.utf8(raw),format,TextPlaylistNative.utf8(defaultArtist));rows.clear();saved="";page=0;
            for(int i=1;i+2<parsed.length;i+=3){Row r=new Row();r.artist=parsed[i];r.title=parsed[i+1];r.original=parsed[i+2];rows.add(r);}
            if(rows.isEmpty()){state("Nenhuma música reconhecida. Confira o texto.");return;}step=1;state(rows.size()+" músicas • "+(parsed[0].isEmpty()?"revise as gravações":"artista detectado: "+parsed[0]));render();search();
        }catch(Exception error){state(error.getMessage());}
    }
    private int selectedCount(){int n=0;for(Row r:rows)if(r.selected&&r.choice>=0)n++;return n;}
    private void review(){
        LinearLayout content=ui.column();content.addView(ui.title("Confira as gravações",22));LinearLayout controls=ui.row();ui.weight(controls,action("Buscar pendentes",this::search));TextView stop=ui.action("Interromper",()->{if(!syncing){++generation;busy=false;state("Busca interrompida. Resultados já obtidos foram mantidos.");render();}});stop.setEnabled(busy&&!syncing);controls.addView(stop);content.addView(controls);
        content.addView(ui.text("Correspondências duvidosas ficam desmarcadas. Escolha a versão ou corrija a busca antes de salvar.",13,Ui.MUTED));
        int end=Math.min(rows.size(),page*25+25);for(int n=page*25;n<end;n++){final int index=n;Row r=rows.get(n);LinearLayout card=ui.column();ui.pad(card,10,8);ui.rounded(card,Ui.CARD,12);LinearLayout heading=ui.row();CheckBox include=new CheckBox(activity);include.setChecked(r.selected);include.setEnabled(!busy&&r.choice>=0);include.setContentDescription("Incluir "+r.title);include.setOnClickListener(v->{r.selected=include.isChecked();r.status=r.selected?"Revisada":"Ignorada";render();});heading.addView(include);ui.weight(heading,ui.title((n+1)+". "+r.title,16));card.addView(heading);card.addView(ui.text(r.artist+" • "+r.status,13,r.selected?Ui.BLUE:Ui.MUTED));
            if(r.choice>=0&&r.choice<r.candidates.length()){JSONObject t=r.candidates.optJSONObject(r.choice);card.addView(ui.text(t.optString("title")+" · "+t.optString("artist")+"\n"+t.optString("album"),13,Ui.TEXT));}
            LinearLayout buttons=ui.row();ui.weight(buttons,action("Versão",()->choose(index)));ui.weight(buttons,action("Editar busca",()->edit(index)));buttons.addView(ui.icon("up","Mover para cima",()->move(index,-1)));buttons.addView(ui.icon("down","Mover para baixo",()->move(index,1)));card.addView(buttons);content.addView(card);ui.space(content,8);
        }
        LinearLayout pages=ui.row();TextView prev=action("←",()->{page--;render();});prev.setEnabled(!busy&&page>0);pages.addView(prev);ui.weight(pages,ui.text("Página "+(page+1)+" / "+Math.max(1,(rows.size()+24)/25),13,Ui.MUTED));TextView next=action("→",()->{page++;render();});next.setEnabled(!busy&&end<rows.size());pages.addView(next);content.addView(pages);scroll(content);
    }
    private void choose(int n){Row r=rows.get(n);if(r.candidates.length()==0){state("Edite a busca ou tente as faixas pendentes.");return;}String[] names=new String[r.candidates.length()];for(int i=0;i<names.length;i++){JSONObject t=r.candidates.optJSONObject(i);names[i]=t.optString("title")+"\n"+t.optString("artist")+" • "+t.optString("album");}new AlertDialog.Builder(activity).setTitle("Escolher gravação").setItems(names,(d,i)->{r.choice=i;r.selected=true;r.status="Revisada";render();}).setNegativeButton("Cancelar",null).show();}
    private void edit(int n){Row r=rows.get(n);LinearLayout content=ui.column();EditText a=field("Artista",r.artist,300,false),t=field("Música",r.title,300,false);content.addView(a);content.addView(t);new AlertDialog.Builder(activity).setTitle("Corrigir busca").setView(content).setNegativeButton("Cancelar",null).setPositiveButton("Salvar",(d,i)->{if(t.getText().toString().trim().isEmpty())return;r.artist=a.getText().toString().trim();r.title=t.getText().toString().trim();r.choice=-1;r.selected=false;r.candidates=new JSONArray();r.status="Aguardando";render();}).show();}
    private void move(int index,int delta){if(busy||index+delta<0||index+delta>=rows.size())return;Collections.swap(rows,index,index+delta);render();}
    private void search(){if(busy)return;++generation;busy=true;state("Buscando no Spotify…");render();nextSearch(0,0);}
    private void nextSearch(int index,int attempt){
        if(index>=rows.size()){busy=false;state("Revisão pronta • "+selectedCount()+" / "+rows.size()+" músicas selecionadas.");render();return;}
        Row row=rows.get(index);if(attempt==0&&row.choice>=0){nextSearch(index+1,0);return;}
        String[] queries=TextPlaylistNative.queries(TextPlaylistNative.utf8(row.artist),TextPlaylistNative.utf8(row.title));
        if(attempt>=queries.length){row.status="Não encontrada";scheduleNext(index);return;}
        int serial=generation;state("Buscando "+(index+1)+" / "+rows.size()+" • "+row.artist+" — "+row.title);
        if(fixture){main.postDelayed(()->{if(!closed&&generation==serial)acceptSearch(index,attempt,fixtureSearch(row));},30);return;}
        api.importCall("GET","search?type=track&limit=10&q="+SpotifyClient.encode(queries[attempt]),null,null,result->{
            acceptSearch(index,attempt,result);
        },error->{busy=false;row.status=error;state(error+". Confira a sessão e tente as pendentes.");render();},()->!closed&&generation==serial);
    }
    private void acceptSearch(int index,int attempt,JSONObject result){Row row=rows.get(index);
            JSONArray items=MusicStore.array(MusicStore.object(result,"tracks"),"items"),candidates=new JSONArray();ArrayList<String> titles=new ArrayList<>(),artists=new ArrayList<>();ArrayList<Integer> popularity=new ArrayList<>();
            for(Object v:MusicStore.values(items)){JSONObject item=(JSONObject)v;if(!item.optBoolean("is_playable",true)||!item.optString("uri").startsWith("spotify:track:"))continue;JSONObject t=MusicStore.spotifyTrack(item);candidates.put(t);titles.add(t.optString("title"));artists.add(t.optString("artist"));popularity.add(item.optInt("popularity",-1));if(candidates.length()==10)break;}
            if(candidates.length()==0){nextSearch(index,attempt+1);return;}
            int[] pop=new int[popularity.size()];for(int i=0;i<pop.length;i++)pop[i]=popularity.get(i);
            double[] ranked=TextPlaylistNative.rank(TextPlaylistNative.utf8(row.artist),TextPlaylistNative.utf8(row.title),titles.toArray(new String[0]),artists.toArray(new String[0]),pop);JSONArray sorted=new JSONArray();for(int i=0;i<ranked.length;i+=3)sorted.put(candidates.optJSONObject((int)ranked[i]));
            row.candidates=sorted;row.choice=0;row.selected=ranked[2]>.5;row.status=row.selected?"Encontrada":"Revisar versão";scheduleNext(index);
    }
    private void scheduleNext(int index){int serial=generation;main.postDelayed(()->{if(!closed&&busy&&!syncing&&generation==serial)nextSearch(index+1,0);},180);}
    private void details(){LinearLayout content=ui.column();content.addView(ui.title("A sua identidade",23));nameInput=field("Nome da playlist",name,100,false);descriptionInput=field("Descrição (opcional)",description,300,true);descriptionInput.setMinLines(3);content.addView(nameInput);content.addView(descriptionInput);
        ImageView image=new ImageView(activity);image.setScaleType(ImageView.ScaleType.FIT_CENTER);if(!cover.isEmpty()){byte[] bytes=Base64.decode(cover.substring(cover.indexOf(',')+1),Base64.DEFAULT);image.setImageBitmap(BitmapFactory.decodeByteArray(bytes,0,bytes.length));}else image.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));content.addView(image,new LinearLayout.LayoutParams(-1,ui.dp(180)));
        LinearLayout choices=ui.row();ui.weight(choices,action("Escolher capa…",()->{collectDetails();pendingCover=new WeakReference<>(this);Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("image/*").addCategory(Intent.CATEGORY_OPENABLE);activity.startActivityForResult(i,COVER_REQUEST);}));ui.weight(choices,action("Remover capa",()->{collectDetails();cover="";render();}));content.addView(choices);deduplicateInput=new CheckBox(activity);deduplicateInput.setText("Remover músicas repetidas");deduplicateInput.setTextColor(Ui.TEXT);deduplicateInput.setChecked(deduplicate);content.addView(deduplicateInput);content.addView(ui.text("A playlist é salva neste aparelho primeiro. Você escolhe depois se deseja copiá-la para sua conta Spotify.",14,Ui.MUTED));scroll(content);
    }
    private void collectDetails(){if(step==2&&nameInput!=null){name=nameInput.getText().toString().trim();description=descriptionInput.getText().toString();deduplicate=deduplicateInput.isChecked();}}
    private void saveLocal(){collectDetails();JSONArray selected=new JSONArray();HashSet<String> seen=new HashSet<>();for(Row r:rows)if(r.selected&&r.choice>=0){JSONObject t=r.candidates.optJSONObject(r.choice);if(t!=null&&(!deduplicate||seen.add(t.optString("uri"))))selected.put(t);}try{saved=store.importPlaylist(name,description,cover,selected);step=3;busy=true;state("Confirmando gravação local…");render();int serial=generation;store.persist(ok->{if(!closed&&generation==serial)finish(ok?"Playlist salva no Tonalify. Deseja salvar também na conta Spotify?":"A gravação local falhou. Confira o espaço em disco e exporte backup antes de enviar.");});}catch(Exception e){state(e.getMessage());}}
    private JSONObject saved(){return store.playlist(saved);}
    private void patch(Object... values){JSONObject p=new JSONObject();for(int i=0;i<values.length;i+=2)MusicStore.put(p,String.valueOf(values[i]),values[i+1]);store.patchPlaylist(saved,p);}
    private void complete(){JSONObject p=saved();LinearLayout content=ui.column();content.addView(ui.title(p.optString("name"),25));content.addView(ui.text(p.optString("description"),15,Ui.MUTED));content.addView(ui.text(MusicStore.array(p,"tracks").length()+" músicas • salvas neste aparelho",15,Ui.BLUE));content.addView(action("Ouvir / abrir playlist local",()->{dialog.dismiss();MusicPanel.showPlaylist(activity,api,saved);}));ui.space(content,20);content.addView(ui.title("Salvar também no Spotify?",22));visibilityInput=spinner(new String[]{"Privada","Pública"},p.optBoolean("spotifyPublic")?1:0);visibilityInput.setEnabled(!busy&&p.optString("spotifyId").isEmpty());content.addView(visibilityInput);content.addView(action(p.optString("spotifyId").isEmpty()?"Salvar também no Spotify":"Concluir / conferir envio",()->sync(visibilityInput.getSelectedItemPosition()==1)));
        if(message.contains("403")||message.contains("401")||message.contains("Autorize"))content.addView(action("Autorizar permissões…",()->activity.authorizeTextPlaylist()));
        if(p.optBoolean("cloudUncertain")&&p.optString("spotifyId").isEmpty())content.addView(action("Já conferi a conta; permitir nova criação…",()->new AlertDialog.Builder(activity).setTitle("Conferiu sua Biblioteca Spotify?").setMessage("A resposta pode ter se perdido após a criação. Só continue se conferiu a conta e deseja permitir uma nova playlist.").setNegativeButton("Cancelar",null).setPositiveButton("Conferi",(d,w)->{patch("cloudUncertain",false);state("Verificação confirmada. Solicite uma nova cópia se desejar.");render();}).show()));
        content.addView(ui.text("O envio é uma cópia. Sua coleção local permanece salva se a rede ou a API falhar. A capa exige permissão de imagens na conta.",13,Ui.MUTED));scroll(content);
    }
    private void call(String method,String path,String payload,String mime,java.util.function.Consumer<JSONObject> result,java.util.function.Consumer<String> failure){int serial=generation;Runnable send=()->{if(closed||generation!=serial)return;if(fixture){main.postDelayed(()->{if(!closed&&generation==serial)fixtureCall(method,path,payload,mime,result,failure);},30);return;}api.importCall(method,path,payload,mime,result,failure,()->!closed&&generation==serial);};if(method.equals("GET"))send.run();else store.persist(ok->{if(closed||generation!=serial)return;if(ok)send.run();else finish("Não foi possível confirmar a gravação local. O envio foi interrompido. Confira o espaço em disco e faça backup.");});}
    private JSONObject fixtureTrack(String artist,String title)throws Exception{return new JSONObject().put("uri","spotify:track:"+String.format(Locale.ROOT,"%022d",Math.abs((long)title.hashCode()))).put("name",title).put("artists",new JSONArray().put(new JSONObject().put("name",artist))).put("album",new JSONObject().put("name","Coleção de teste nativo"));}
    private JSONObject fixtureSearch(Row row){try{return new JSONObject().put("tracks",new JSONObject().put("items",new JSONArray().put(fixtureTrack("Karaoke Tribute",row.title).put("popularity",99)).put(fixtureTrack(row.artist,row.title))));}catch(Exception impossible){throw new AssertionError(impossible);}}
    private void fixtureCall(String method,String path,String payload,String mime,java.util.function.Consumer<JSONObject> result,java.util.function.Consumer<String> failure){try{
        JSONObject out=new JSONObject();if(method.equals("POST")&&path.equals("me/playlists")){fixtureCreates++;fixtureWrites++;fixturePublic=new JSONObject(payload).optBoolean("public");out.put("id","abcdefghijklmnopqrstuv");}
        else if(method.equals("GET")){JSONArray items=new JSONArray();for(String uri:fixtureRemote)items.put(new JSONObject().put("item",new JSONObject().put("uri",uri)));out.put("items",items);}
        else if(path.endsWith("/items")){fixtureWrites++;if(method.equals("PUT"))fixtureRemote.clear();for(Object uri:MusicStore.values(new JSONObject(payload).getJSONArray("uris")))fixtureRemote.add(String.valueOf(uri));if(activity.getIntent().getBooleanExtra("textPlaylistDropBatch",false)&&!fixtureDropped){fixtureDropped=true;failure.accept("Spotify HTTP 503: resposta de teste perdida");fixtureReceipt();return;}}
        else if(path.endsWith("/images")){fixtureWrites++;fixtureMime=mime;if(payload.length()>256*1024)throw new IOException("fixture cover limit");}
        result.accept(out);fixtureReceipt();
    }catch(Exception error){failure.accept(error.getMessage());}}
    private void fixtureReceipt(){if(!fixture)return;try{JSONObject report=new JSONObject().put("fixture",true).put("native_cpp",true).put("real_spotify_writes",0).put("local_id",saved).put("local_tracks",MusicStore.array(saved(),"tracks").length()).put("description",saved().optString("description")).put("has_cover",!saved().optString("cover").isEmpty()).put("cloud_creates",fixtureCreates).put("cloud_writes",fixtureWrites).put("cloud_items",fixtureRemote.size()).put("cover_mime",fixtureMime).put("public",fixturePublic).put("uncertain",saved().optBoolean("cloudUncertain")).put("message",message);java.nio.file.Files.write(new File(activity.getExternalFilesDir(null),"text-playlist-tests.json").toPath(),report.toString(2).getBytes(java.nio.charset.StandardCharsets.UTF_8));}catch(Exception ignored){}}

    private void finish(String text){busy=syncing=false;state(text);render();fixtureReceipt();}
    private void sync(boolean publicPlaylist){if(busy||saved.isEmpty())return;JSONObject p=saved();upload=store.resolve(MusicStore.array(p,"tracks"));if(upload.length()==0)return;cloudId=p.optString("spotifyId");cursor=Math.max(0,Math.min(upload.length(),p.optInt("syncedCount")));++generation;busy=syncing=true;state("Copiando para o Spotify…");render();
        if(!cloudId.isEmpty()){if(!cloudId.matches("[A-Za-z0-9]{22}")){finish("Identificação da playlist Spotify inválida");return;}verifyPages=0;if(p.optBoolean("cloudUncertain"))verify("playlists/"+cloudId+"/items?limit=50",new ArrayList<>());else batch();return;}
        if(p.optBoolean("cloudUncertain")){finish("O envio anterior não confirmou a criação. Confira sua Biblioteca Spotify antes de criar outra cópia.");return;}
        JSONObject body=new JSONObject();MusicStore.put(body,"name",p.optString("name"));MusicStore.put(body,"description",p.optString("description"));MusicStore.put(body,"public",publicPlaylist);patch("cloudUncertain",true,"spotifyPublic",publicPlaylist);
        call("POST","me/playlists",body.toString(),"application/json",result->{String id=result.optString("id");if(!id.matches("[A-Za-z0-9]{22}")){finish("O Spotify não confirmou a identificação da playlist. Confira a conta antes de repetir.");return;}cloudId=id;cursor=0;patch("spotifyId",id,"syncedCount",0,"cloudUncertain",false);batch();},error->{if(error.matches("(?s).*HTTP 4[0-9]{2}.*"))patch("cloudUncertain",false);finish(error+". Sua playlist local continua salva.");});
    }
    private void verify(String path,ArrayList<String> remote){if(++verifyPages>12){finish("Não foi possível conferir a paginação. Nenhuma faixa extra foi enviada.");return;}call("GET",path,null,null,result->{for(Object v:MusicStore.values(MusicStore.array(result,"items"))){JSONObject item=(JSONObject)v,t=item.optJSONObject("item");if(t==null)t=item.optJSONObject("track");remote.add(t==null?"":t.optString("uri"));}if(remote.size()>upload.length()){finish("A playlist Spotify foi alterada. Confira seu conteúdo antes de continuar.");return;}String next=result.optString("next");if(!next.isEmpty()&&!next.equals("null")){verify(next,remote);return;}for(int i=0;i<remote.size();i++)if(!remote.get(i).equals(upload.optJSONObject(i).optString("uri"))){finish("A ordem no Spotify mudou. Nenhuma faixa extra foi enviada.");return;}cursor=remote.size();patch("syncedCount",cursor,"cloudUncertain",false);batch();},this::finish);}
    private void batch(){if(cursor>=upload.length()){patch("cloudComplete",true);uploadCover();return;}JSONArray uris=new JSONArray();for(int i=cursor;i<Math.min(cursor+100,upload.length());i++)uris.put(upload.optJSONObject(i).optString("uri"));JSONObject body=new JSONObject();MusicStore.put(body,"uris",uris);int end=cursor+uris.length();patch("cloudUncertain",true);state("Spotify • enviando "+(cursor+1)+"–"+end+" / "+upload.length());call(cursor==0?"PUT":"POST","playlists/"+cloudId+"/items",body.toString(),"application/json",r->{cursor=end;patch("syncedCount",cursor,"cloudUncertain",false);batch();},error->finish(error+". Sua playlist local está salva. Na próxima tentativa, o conteúdo será conferido antes de continuar."));}
    private void uploadCover(){if(cover.isEmpty()||saved().optBoolean("coverSynced")){finish("Playlist salva no Tonalify e no Spotify.");return;}call("PUT","playlists/"+cloudId+"/images",cover.substring(cover.indexOf(',')+1),"image/jpeg",r->{patch("coverSynced",true);finish("Playlist e capa salvas no Tonalify e no Spotify.");},error->finish("Músicas copiadas. Capa pendente: "+error+". Autorize imagens e conclua o envio novamente."));}
    static void photoResult(Activity a,int request,int result,Intent data){if(request!=COVER_REQUEST||result!=Activity.RESULT_OK||data==null||data.getData()==null)return;TextPlaylistWizard w=pendingCover.get();if(w==null||w.closed||w.activity!=a)return;Uri uri=data.getData();w.busy=true;w.state("Preparando capa…");int serial=w.generation;w.api.worker.execute(()->{try{
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=a.getContentResolver().openInputStream(uri)){byte[] block=new byte[8192];int n;while((n=in.read(block))!=-1){if(bytes.size()+n>16*1024*1024)throw new IOException("Imagem maior que 16 MiB");bytes.write(block,0,n);}}byte[] source=bytes.toByteArray();BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(source,0,source.length,o);if(o.outWidth<=0||o.outHeight<=0||(long)o.outWidth*o.outHeight>16000000)throw new IOException("Escolha uma imagem com até 16 megapixels");o.inSampleSize=1;while(o.outWidth/o.inSampleSize>512||o.outHeight/o.inSampleSize>512)o.inSampleSize*=2;o.inJustDecodeBounds=false;Bitmap bitmap=BitmapFactory.decodeByteArray(source,0,source.length,o);if(bitmap==null)throw new IOException("Não foi possível ler a imagem");ByteArrayOutputStream jpeg=new ByteArrayOutputStream();bitmap.compress(Bitmap.CompressFormat.JPEG,80,jpeg);bitmap.recycle();String encoded=Base64.encodeToString(jpeg.toByteArray(),Base64.NO_WRAP);if(encoded.length()>256*1024)throw new IOException("Capa muito grande; escolha uma imagem menor");a.runOnUiThread(()->{if(!w.closed&&w.generation==serial){w.cover="data:image/jpeg;base64,"+encoded;w.busy=false;w.state("Capa pronta");w.render();}});
    }catch(Exception error){a.runOnUiThread(()->{if(!w.closed){w.busy=false;w.state(error.getMessage());w.render();}});}});}
}
