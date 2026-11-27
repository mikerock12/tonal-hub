package app.tonal.android;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.net.Uri;
import android.os.*;
import android.text.InputFilter;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.lang.ref.WeakReference;
import java.util.*;

/** Native views and existing Spotify transport; business rules execute in C++. */
final class TextPlaylistWizard {
    static final int COVER_REQUEST=184;
    private static WeakReference<TextPlaylistWizard> pendingCover=new WeakReference<>(null);
    private final MainActivity activity;private final SpotifyClient api;private final MusicStore store;private final Ui ui;private final Artwork artwork;
    private final Handler main=new Handler(Looper.getMainLooper());private Dialog dialog;private LinearLayout body,footer;
    private TextView notice;private ProgressBar progress;private EditText sourceInput,artistInput,nameInput,descriptionInput;private Spinner formatInput,visibilityInput,destinationInput;private CheckBox deduplicateInput;
    private String raw="",defaultArtist="",name="Minha playlist de texto",description="",cover="",saved="",cloudId="",targetId="",message="Cole uma lista, setlist ou músicas de um show.";
    private int format,step,page,generation,cursor,verifyPages;private boolean busy,syncing,closed,deduplicate;private String filter="all";private long lookup;private int lookupId=-1,onlyId=-1,nextId;private JSONArray pool=new JSONArray();private String stopReason="";private final HashMap<String,String[]> artistCache=new HashMap<>();
    private LinearLayout reviewContent;private ScrollView reviewScroll;private TextView reviewContinue;
    private final ArrayList<Row> rows=new ArrayList<>();private JSONArray upload=new JSONArray();private final ArrayList<JSONObject> targets=new ArrayList<>();
    private final boolean fixture;
    private int fixtureCreates,fixtureWrites,fixturePuts;private boolean fixtureDropped,fixturePublic;private String fixtureMime="";private final ArrayList<String> fixtureRemote=new ArrayList<>();
    private static final class Row {int id;String artist,title,original,status="Aguardando",match="PENDING",reason="",section="waiting",lookup="waiting",manualUri="";JSONArray candidates=new JSONArray();JSONObject manualTrack;int choice=-1;boolean selected,manual,ignored;}
    static void show(MainActivity a,SpotifyClient api){new TextPlaylistWizard(a,api).show();}
    static void fromSetlist(MainActivity a,SpotifyClient api,String text,String title,String source){TextPlaylistWizard w=new TextPlaylistWizard(a,api);w.raw=text;w.name=title;w.description=source.substring(0,Math.min(300,source.length()));w.message="Setlist importado do setlist.fm. Revise as gravações antes de salvar.";w.show();}
    static void resume(MainActivity a,SpotifyClient api,String id){TextPlaylistWizard w=new TextPlaylistWizard(a,api);JSONObject p=w.store.playlist(id);if(p.length()==0)return;w.saved=id;w.cover=p.optString("cover");w.name=p.optString("name");w.description=p.optString("description");w.step=3;w.message="Playlist salva neste aparelho. Você pode concluir a cópia para o Spotify.";w.show();}
    private TextPlaylistWizard(MainActivity a,SpotifyClient client){activity=a;api=client;store=MusicStore.get(a);ui=new Ui(a);artwork=new Artwork(a);fixture=BuildConfig.DEBUG&&a.getIntent().getBooleanExtra("textPlaylistFixture",false);if(fixture){raw="Veruca Salt - Seether\nL7 - Shove\nNirvana - About a Girl";if(a.getIntent().getBooleanExtra("textPlaylistMany",false)){StringBuilder many=new StringBuilder();for(int i=0;i<30;i++){if(i>0)many.append('\n');many.append(i==1?"L7":"Banda "+i).append(" - ").append(i==1?"Shove":"Música "+i);}raw=many.toString();}Bitmap b=Bitmap.createBitmap(48,48,Bitmap.Config.ARGB_8888);b.eraseColor(Ui.BLUE);ByteArrayOutputStream out=new ByteArrayOutputStream();b.compress(Bitmap.CompressFormat.JPEG,80,out);b.recycle();cover="data:image/jpeg;base64,"+Base64.encodeToString(out.toByteArray(),Base64.NO_WRAP);if(a.getIntent().getBooleanExtra("textPlaylistExisting",false))fixtureRemote.add("spotify:track:AAAAAAAAAAAAAAAAAAAAAA");}}
    private void show(){
        dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,16,10);Ui.insets(root);
        LinearLayout header=ui.row();ui.weight(header,ui.title("Text to Tonalify",24));header.addView(ui.icon("close","Fechar",()->{if(!syncing)dialog.dismiss();}));root.addView(header);
        notice=ui.text(message,14,Ui.MUTED);ui.pad(notice,0,12);root.addView(notice);progress=new ProgressBar(activity,null,android.R.attr.progressBarStyleHorizontal);progress.setIndeterminate(true);root.addView(progress,new LinearLayout.LayoutParams(-1,ui.dp(4)));
        body=ui.column();root.addView(body,new LinearLayout.LayoutParams(-1,0,1));footer=ui.row();root.addView(footer);dialog.setContentView(root);
        dialog.setOnDismissListener(d->{closed=true;++generation;freeLookup();main.removeCallbacksAndMessages(null);store.flush();artwork.close();});dialog.show();dialog.getWindow().setLayout(-1,-1);dialog.getWindow().setStatusBarColor(Ui.BG);dialog.getWindow().setNavigationBarColor(Ui.BG);render();loadTargets();
    }
    private void loadTargets(){
        if(fixture){try{targets.add(new JSONObject().put("id","abcdefghijklmnopqrstuv").put("name","Playlist existente de teste"));}catch(JSONException ignored){}return;}
        if(api.loggedIn())loadTargetPage("me/playlists?limit=50",0);
    }
    private void loadTargetPage(String path,int pages){
        if(closed||pages>=10)return;
        api.get(path,result->{if(closed)return;for(Object value:MusicStore.values(MusicStore.array(result,"items"))){JSONObject item=(JSONObject)value;String id=item.optString("id");if(id.matches("[A-Za-z0-9]{22}")){boolean known=false;for(JSONObject target:targets)if(target.optString("id").equals(id)){known=true;break;}if(!known)targets.add(item);}}
            String next=result.optString("next");if(!next.isEmpty()&&!next.equals("null"))loadTargetPage(next,pages+1);else if(step==2)render();},error->{});
    }
    private void state(String text){message=text;if(notice!=null)notice.setText(text);if(progress!=null)progress.setVisibility(busy?android.view.View.VISIBLE:android.view.View.GONE);if(dialog!=null)dialog.setCancelable(!syncing);}
    private Spinner spinner(String[] values,int selected){Spinner s=new Spinner(activity);s.setAdapter(new ArrayAdapter<>(activity,android.R.layout.simple_spinner_dropdown_item,values));s.setSelection(selected);return s;}
    private EditText field(String hint,String value,int max,boolean multiline){EditText f=new EditText(activity);f.setHint(hint);f.setHintTextColor(Ui.MUTED);f.setTextColor(Ui.TEXT);f.setText(value);f.setTextSize(16);f.setSingleLine(!multiline);f.setFilters(new InputFilter[]{new InputFilter.LengthFilter(max)});return f;}
    private ScrollView scroll(LinearLayout content){ScrollView s=new ScrollView(activity);s.addView(content);body.addView(s,new LinearLayout.LayoutParams(-1,-1));return s;}
    private TextView action(String label,Runnable run){TextView b=ui.action(label,run);b.setEnabled(!busy);return b;}
    private void render(){if(closed)return;body.removeAllViews();footer.removeAllViews();state(message);
        if(step==0)paste();else if(step==1)review();else if(step==2)details();else complete();
        if(step>0&&step<3)ui.weight(footer,action("Voltar",()->{collectDetails();--step;render();}));
        if(step==0)ui.weight(footer,action("Interpretar e buscar",this::parse));
        else if(step==1){reviewContinue=action("Personalizar · "+selectedCount(),()->{step=2;render();});reviewContinue.setEnabled(!busy&&selectedCount()>0);ui.weight(footer,reviewContinue);}
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
        try{String[] parsed=TextPlaylistNative.parse(TextPlaylistNative.utf8(raw),format,TextPlaylistNative.utf8(defaultArtist));freeLookup();rows.clear();artistCache.clear();saved="";targetId="";page=0;filter="all";
            for(int i=1;i+2<parsed.length;i+=3){Row r=new Row();r.id=++nextId;r.artist=parsed[i];r.title=parsed[i+1];r.original=parsed[i+2];rows.add(r);}
            if(rows.isEmpty()){state("Nenhuma música reconhecida. Confira o texto.");return;}step=1;state(rows.size()+" músicas • "+(parsed[0].isEmpty()?"revise as gravações":"artista detectado: "+parsed[0]));render();search();
        }catch(Exception error){state(error.getMessage());}
    }
    private int selectedCount(){int n=0;for(Row r:rows)if(r.selected&&r.choice>=0&&r.choice<r.candidates.length())n++;return n;}
    private static final String[] SECTIONS={"all","pending","selected","equivalent","alternate","wrongArtist","notFound","unavailable","error","ignored","waiting"};
    private static final String[] SECTION_LABELS={"Todas","Pendências","Encontradas e selecionadas","Equivalentes para confirmar","Versões alternativas","Só artista errado","Não encontradas","Indisponíveis na região","Erro / busca incompleta","Ignoradas","Aguardando busca"};
    /** Same rules as TextPlaylistImport::sectionOf on Windows; counters derive from these fields only. */
    private static String section(Row r){
        if(r.ignored)return "ignored";if(r.selected&&r.choice>=0&&r.choice<r.candidates.length())return "selected";
        switch(r.match){case "EQUIVALENT_MATCH":case "AMBIGUOUS":case "EXACT_MATCH":return "equivalent";case "ALTERNATE_VERSION":return "alternate";case "UNAVAILABLE_IN_MARKET":return "unavailable";
            case "LOOKUP_ERROR":case "SEARCH_INCOMPLETE":return "error";
            case "NOT_FOUND":case "WRONG_ARTIST":case "WRONG_SONG":case "COVER_OR_OTHER_ARTIST":for(Object v:MusicStore.values(r.candidates))if(!((JSONObject)v).optBoolean("requestedArtist"))return "wrongArtist";return "notFound";
            default:return "waiting";}
    }
    private void refresh(Row r){
        r.section=section(r);
        if(r.manual)r.status=r.selected?"Escolha manual":"Ignorada";
        else if(r.match.equals("PENDING"))r.status=r.lookup.equals("searching")?"Buscando…":"Aguardando";
        else{r.status=TextPlaylistNative.label(TextPlaylistNative.utf8(r.match));if(r.match.equals("EQUIVALENT_MATCH"))r.status+=r.selected?" · selecionada":" · confirme";}
    }
    private int count(String section){int n=0;for(Row r:rows)if(section.equals("all")||section.equals(r.section)||(section.equals("pending")&&!r.section.equals("selected")&&!r.section.equals("ignored")))n++;return n;}
    private ArrayList<Integer> reviewIndices(){ArrayList<Integer> indices=new ArrayList<>();for(int i=0;i<rows.size();i++){Row r=rows.get(i);if(filter.equals("all")||filter.equals(r.section)||(filter.equals("pending")&&!r.section.equals("selected")&&!r.section.equals("ignored")))indices.add(i);}return indices;}
    private int indexOf(int id){for(int i=0;i<rows.size();i++)if(rows.get(i).id==id)return i;return -1;}
    private int statusColor(Row r){return r.selected?Ui.BLUE:r.section.equals("error")?0xffffb36b:r.section.equals("wrongArtist")||r.section.equals("notFound")?0xfff3c98a:Ui.MUTED;}
    private void updateReviewCard(int index){
        if(reviewContent==null||index<0)return;Row row=rows.get(index);LinearLayout card=reviewContent.findViewWithTag("review-row-"+index);
        if(card!=null){CheckBox include=card.findViewWithTag("review-check-"+index);if(include!=null){include.setChecked(row.selected);include.setEnabled(!syncing&&row.choice>=0);}
            TextView status=card.findViewWithTag("review-status-"+index);if(status!=null){status.setText(row.artist+" • "+row.status);status.setTextColor(statusColor(row));}
            TextView reason=card.findViewWithTag("review-reason-"+index);if(reason!=null){reason.setText(reasonOf(row));reason.setVisibility(reasonOf(row).isEmpty()?View.GONE:View.VISIBLE);}
            TextView title=card.findViewWithTag("review-track-"+index),album=card.findViewWithTag("review-album-"+index);ImageView cover=card.findViewWithTag("review-cover-"+index);
            if(row.choice>=0&&row.choice<row.candidates.length()){JSONObject track=row.candidates.optJSONObject(row.choice);if(title!=null){title.setVisibility(View.VISIBLE);title.setText(track.optString("title")+" · "+track.optString("artist"));}if(album!=null){album.setVisibility(View.VISIBLE);album.setText(track.optString("album"));}if(cover!=null)artwork.load(cover,track.optString("image"));}
            else{if(title!=null)title.setVisibility(View.GONE);if(album!=null)album.setVisibility(View.GONE);}}
        if(reviewContinue!=null){reviewContinue.setText("Personalizar · "+selectedCount());reviewContinue.setEnabled(!busy&&selectedCount()>0);}
    }
    private String reasonOf(Row r){if(r.choice>=0&&r.choice<r.candidates.length()){String reason=r.candidates.optJSONObject(r.choice).optString("reason");if(!reason.isEmpty())return reason;}return r.reason;}
    private void review(){
        reviewContent=ui.column();reviewContent.addView(ui.title("Confira as gravações",22));LinearLayout controls=ui.row();ui.weight(controls,action("Buscar pendentes",this::search));TextView stop=ui.action("Interromper",()->{if(!syncing){cancelSearch();state("Busca interrompida. Resultados já obtidos foram mantidos.");render();}});stop.setEnabled(busy&&!syncing);controls.addView(stop);reviewContent.addView(controls);
        reviewContent.addView(ui.text(selectedCount()+" selecionadas · "+count("pending")+" pendentes · "+count("ignored")+" ignoradas de "+rows.size(),14,Ui.TEXT));
        boolean pendingOnly=filter.equals("pending");TextView toggle=action(pendingOnly?"Mostrar todas as referências":"Só pendências ("+count("pending")+")",()->{filter=filter.equals("pending")?"all":"pending";page=0;render();});toggle.setEnabled(!busy);reviewContent.addView(toggle);
        ArrayList<String> labels=new ArrayList<>();int current=0;for(int i=0;i<SECTIONS.length;i++){labels.add(SECTION_LABELS[i]+" ("+count(SECTIONS[i])+")");if(SECTIONS[i].equals(filter))current=i;}
        Spinner sections=spinner(labels.toArray(new String[0]),current);sections.setTag("review-sections");sections.setEnabled(!busy);final int selectedSection=current;
        sections.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener(){public void onNothingSelected(android.widget.AdapterView<?> p){}public void onItemSelected(android.widget.AdapterView<?> p,View v,int n,long id){if(n!=selectedSection){filter=SECTIONS[n];page=0;render();}}});reviewContent.addView(sections);
        reviewContent.addView(ui.text("Encontradas e equivalentes comprovadas ficam marcadas. Versões alternativas, ambíguas e resultados de outros artistas ficam desmarcados; homônimos nunca contam como encontrados.",13,Ui.MUTED));
        ArrayList<Integer> indices=reviewIndices();int pagesCount=Math.max(1,(indices.size()+24)/25);page=Math.max(0,Math.min(page,pagesCount-1));int end=Math.min(indices.size(),page*25+25);
        if(indices.isEmpty())reviewContent.addView(ui.text(filter.equals("pending")?"Nenhuma pendência.":"Nenhuma referência nesta seção.",15,Ui.MUTED));
        for(int n=page*25;n<end;n++){final int index=indices.get(n);Row r=rows.get(index);LinearLayout card=ui.column();card.setTag("review-row-"+index);ui.pad(card,10,8);ui.rounded(card,Ui.CARD,12);LinearLayout heading=ui.row();
            CheckBox include=new CheckBox(activity);include.setTag("review-check-"+index);include.setChecked(r.selected);include.setEnabled(!syncing&&r.choice>=0);include.setContentDescription("Incluir "+r.title);include.setOnClickListener(v->choose(index,r.choice,include.isChecked()));heading.addView(include);
            ImageView cover=new ImageView(activity);cover.setTag("review-cover-"+index);cover.setScaleType(ImageView.ScaleType.CENTER_CROP);ui.rounded(cover,Ui.PANEL,6);heading.addView(cover,new LinearLayout.LayoutParams(ui.dp(48),ui.dp(48)));
            LinearLayout labelsColumn=ui.column();ui.pad(labelsColumn,10,0);TextView title=ui.title((index+1)+". "+r.title,16);title.setMaxLines(2);labelsColumn.addView(title);TextView status=ui.text(r.artist+" • "+r.status,13,statusColor(r));status.setTag("review-status-"+index);labelsColumn.addView(status);ui.weight(heading,labelsColumn);card.addView(heading);
            TextView track=ui.text("",13,Ui.TEXT);track.setTag("review-track-"+index);card.addView(track);TextView album=ui.text("",13,Ui.MUTED);album.setTag("review-album-"+index);card.addView(album);
            TextView reason=ui.text(reasonOf(r),12,Ui.MUTED);reason.setTag("review-reason-"+index);card.addView(reason);
            if(r.choice>=0&&r.choice<r.candidates.length()){JSONObject t=r.candidates.optJSONObject(r.choice);track.setText(t.optString("title")+" · "+t.optString("artist"));album.setText(t.optString("album"));artwork.load(cover,t.optString("image"));}else{track.setVisibility(View.GONE);album.setVisibility(View.GONE);}
            if(reasonOf(r).isEmpty())reason.setVisibility(View.GONE);
            LinearLayout buttons=ui.row();TextView versions=ui.action("Gravações",()->choose(index));versions.setEnabled(!syncing&&r.candidates.length()>0);ui.weight(buttons,versions);ui.weight(buttons,action("Editar busca",()->edit(index)));
            if(r.section.equals("error")||r.section.equals("notFound")||r.section.equals("wrongArtist")||r.manual)ui.weight(buttons,action("Tentar de novo",()->retry(index)));
            else if(!r.ignored){TextView skip=ui.action("Ignorar",()->ignore(index));skip.setEnabled(!syncing);ui.weight(buttons,skip);}
            buttons.addView(ui.icon("up","Mover para cima",()->move(index,-1)));buttons.addView(ui.icon("down","Mover para baixo",()->move(index,1)));card.addView(buttons);reviewContent.addView(card);ui.space(reviewContent,8);
        }
        LinearLayout pages=ui.row();TextView prev=action("←",()->{page--;render();});prev.setEnabled(!busy&&page>0);pages.addView(prev);ui.weight(pages,ui.text("Página "+(page+1)+" / "+pagesCount+" • "+indices.size()+" referências",13,Ui.MUTED));TextView next=action("→",()->{page++;render();});next.setEnabled(!busy&&end<indices.size());pages.addView(next);reviewContent.addView(pages);reviewScroll=scroll(reviewContent);reviewScroll.setTag("textReviewScroll");
    }
    /** "Escolher gravação": the requested artist first, other performers under a labelled header. */
    private void choose(int n){Row r=rows.get(n);if(r.candidates.length()==0){state("Nenhuma gravação para escolher. Edite a busca ou tente de novo.");return;}
        ArrayList<Object> items=new ArrayList<>();boolean header=false;int requested=0;for(Object value:MusicStore.values(r.candidates))if(((JSONObject)value).optBoolean("requestedArtist"))requested++;
        for(int i=0;i<r.candidates.length();i++){JSONObject t=r.candidates.optJSONObject(i);if(!header&&!t.optBoolean("requestedArtist")){items.add(requested==0?"Nenhuma gravação do artista pedido · resultados não correspondentes":"Resultados não correspondentes (outros artistas ou músicas)");header=true;}items.add(i);}
        ArrayAdapter<Object> adapter=new ArrayAdapter<Object>(activity,0,items){
            public int getViewTypeCount(){return 2;}
            public int getItemViewType(int position){return getItem(position) instanceof String?1:0;}
            public boolean isEnabled(int position){return !(getItem(position) instanceof String);}
            public boolean areAllItemsEnabled(){return false;}
            public View getView(int position,View convertView,ViewGroup parent){
                Object item=getItem(position);
                if(item instanceof String){TextView label=convertView instanceof TextView?(TextView)convertView:ui.text("",13,0xfff3c98a);ui.pad(label,12,10);label.setText((String)item);return label;}
                LinearLayout row=(LinearLayout)convertView;ImageView cover;TextView title,detail;if(row==null){row=ui.row();ui.pad(row,8,8);cover=new ImageView(activity);cover.setScaleType(ImageView.ScaleType.CENTER_CROP);ui.rounded(cover,Ui.PANEL,6);row.addView(cover,new LinearLayout.LayoutParams(ui.dp(56),ui.dp(56)));LinearLayout labels=ui.column();ui.pad(labels,12,0);title=ui.title("",15);title.setMaxLines(2);detail=ui.text("",12,Ui.MUTED);detail.setMaxLines(4);labels.addView(title);labels.addView(detail);ui.weight(row,labels);}else{cover=(ImageView)row.getChildAt(0);LinearLayout labels=(LinearLayout)row.getChildAt(1);title=(TextView)labels.getChildAt(0);detail=(TextView)labels.getChildAt(1);}
                int i=(Integer)item;JSONObject track=r.candidates.optJSONObject(i);title.setText((i==r.choice?"✓ ":"")+track.optString("title")+" · "+track.optString("artist"));
                detail.setText(track.optString("matchLabel")+" · "+track.optString("reason")+"\n"+track.optString("album"));detail.setTextColor(track.optBoolean("requestedArtist")?Ui.MUTED:0xfff3c98a);
                cover.setContentDescription("Capa de "+track.optString("album"));artwork.load(cover,track.optString("image"));return row;}
        };
        new AlertDialog.Builder(activity).setTitle("Escolher gravação").setAdapter(adapter,(d,position)->{Object item=items.get(position);if(!(item instanceof Integer))return;int i=(Integer)item;JSONObject t=r.candidates.optJSONObject(i);
            if(t.optBoolean("requestedArtist"))choose(n,i,true);
            else new AlertDialog.Builder(activity).setTitle("Usar resultado de outro artista?").setMessage(t.optString("title")+" · "+t.optString("artist")+"\n\n"+t.optString("reason")+"\n\nIsso não é a gravação pedida ("+r.artist+"). Use só se for mesmo a substituição desejada.").setNegativeButton("Cancelar",null).setPositiveButton("Usar mesmo assim",(x,y)->choose(n,i,true)).show();
        }).setNegativeButton("Cancelar",null).show();}
    /** UserOverride: kept across "Buscar pendentes" and late results until retry or edit. Allowed during a search. */
    private void choose(int n,int candidate,boolean on){if(syncing||n<0||n>=rows.size())return;Row r=rows.get(n);if(candidate<0||candidate>=r.candidates.length())return;r.choice=candidate;r.selected=on;r.manual=true;r.ignored=!on;JSONObject t=r.candidates.optJSONObject(candidate);r.manualUri=t.optString("uri");r.manualTrack=t;refresh(r);updateReviewCard(n);}
    private void ignore(int n){if(syncing||n<0||n>=rows.size())return;Row r=rows.get(n);r.selected=false;r.manual=true;r.ignored=true;refresh(r);if(busy)updateReviewCard(n);else render();}
    private void retry(int n){if(busy||syncing||n<0||n>=rows.size())return;Row r=rows.get(n);r.manual=false;r.ignored=false;r.selected=false;r.manualUri="";r.manualTrack=null;refresh(r);startSearch(r.id);}
    private void edit(int n){Row r=rows.get(n);if(busy)return;LinearLayout content=ui.column();EditText a=field("Artista",r.artist,300,false),t=field("Música",r.title,300,false);content.addView(a);content.addView(t);new AlertDialog.Builder(activity).setTitle("Corrigir busca").setView(content).setNegativeButton("Cancelar",null).setPositiveButton("Salvar",(d,i)->{if(t.getText().toString().trim().isEmpty())return;r.artist=a.getText().toString().trim();r.title=t.getText().toString().trim();r.choice=-1;r.selected=false;r.manual=false;r.ignored=false;r.manualUri="";r.manualTrack=null;r.candidates=new JSONArray();r.match="PENDING";r.reason="";r.lookup="waiting";refresh(r);render();}).show();}
    private void move(int index,int delta){if(busy||index+delta<0||index+delta>=rows.size())return;Collections.swap(rows,index,index+delta);render();}
    private void search(){startSearch(-1);}
    private void startSearch(int only){if(busy||syncing)return;++generation;busy=true;onlyId=only;stopReason="";state("Buscando no Spotify…");render();nextRow(only>=0?Math.max(0,indexOf(only)):0);}
    private void freeLookup(){if(lookup!=0){TextPlaylistNative.lookupFree(lookup);lookup=0;}lookupId=-1;pool=new JSONArray();}
    private void cancelSearch(){++generation;if(lookup!=0){int index=indexOf(lookupId);if(index>=0){rows.get(index).lookup="waiting";refresh(rows.get(index));}}freeLookup();busy=false;onlyId=-1;}
    private void finishSearch(String text){busy=false;onlyId=-1;state(!text.isEmpty()?text:"Revisão pronta • "+selectedCount()+" selecionadas, "+count("pending")+" pendentes, "+count("ignored")+" ignoradas de "+rows.size()+". Homônimos de outros artistas ficam separados.");render();}
    private void nextRow(int index){
        for(;index<rows.size();index++){Row r=rows.get(index);if(onlyId>=0&&r.id!=onlyId)continue;if(onlyId<0&&(r.manual||r.selected))continue;break;}
        if(index>=rows.size()){finishSearch("");return;}
        Row row=rows.get(index);freeLookup();lookup=TextPlaylistNative.lookupCreate(TextPlaylistNative.utf8(row.artist),TextPlaylistNative.utf8(row.title),TextPlaylistNative.utf8(row.original==null?"":row.original));lookupId=row.id;
        String key=TextPlaylistNative.normalize(TextPlaylistNative.utf8(row.artist));String[] cached=artistCache.get(key);if(!key.isEmpty()&&cached!=null)TextPlaylistNative.lookupArtists(lookup,cached,true);
        row.lookup="searching";refresh(row);state("Buscando "+(index+1)+" / "+rows.size()+" • "+row.artist+" — "+row.title);updateReviewCard(index);step();
    }
    private void step(){
        if(lookup==0)return;String[] r=TextPlaylistNative.lookupNext(lookup);if(r==null||r[0].isEmpty()){completeRow();return;}
        StringBuilder url=new StringBuilder(r[1].startsWith("/")?r[1].substring(1):r[1]);for(int i=8;i+1<r.length;i+=2)url.append(i==8?'?':'&').append(r[i]).append('=').append(SpotifyClient.encode(r[i+1]));
        String kind=r[0];String[] album={r[3],r[4],r[5],r[6],r[7]};int id=lookupId,serial=generation;String path=url.toString();
        if(fixture){main.postDelayed(()->{if(!closed&&generation==serial&&lookupId==id)acceptLookup(kind,album,fixtureLookup(kind,path,rows.get(indexOf(id))),null);},20);return;}
        api.importCall("GET",path,null,null,result->{if(lookupId==id)acceptLookup(kind,album,result,null);},error->{if(lookupId==id)acceptLookup(kind,album,null,error==null?"Falha de rede":error);},()->!closed&&generation==serial&&lookupId==id);
    }
    private static int statusOf(String error){java.util.regex.Matcher m=java.util.regex.Pattern.compile("HTTP (\\d{3})").matcher(error);return m.find()?Integer.parseInt(m.group(1)):0;}
    private static String lookupFailure(int status,String error){if(status==401||status==403)return "Sessão Spotify sem permissão (HTTP "+status+"). Autorize novamente e use Buscar pendentes.";if(status==429)return error;if(status==0)return "Falha de rede ou tempo esgotado";return "Spotify HTTP "+status+" na busca";}
    private static String join(ArrayList<String> values){StringBuilder b=new StringBuilder();for(String v:values){if(b.length()>0)b.append('\u001f');b.append(v.replace('\u001f',' '));}return b.toString();}
    private void acceptLookup(String kind,String[] album,JSONObject result,String error){
        if(lookup==0||closed)return;
        if(error!=null){int status=statusOf(error);String message=lookupFailure(status,error);TextPlaylistNative.lookupFail(lookup,status,TextPlaylistNative.utf8(message));if(TextPlaylistNative.lookupFinished(lookup)&&(status==401||status==403||status==429||status==0))stopReason=message;}
        else if(kind.equals("artists")){ArrayList<String> pairs=new ArrayList<>();for(Object v:MusicStore.values(MusicStore.array(MusicStore.object(result,"artists"),"items"))){JSONObject a=(JSONObject)v;pairs.add(a.optString("id"));pairs.add(a.optString("name"));if(pairs.size()>=100)break;}TextPlaylistNative.lookupArtists(lookup,pairs.toArray(new String[0]),false);}
        else if(kind.equals("albums")){ArrayList<String> fields=new ArrayList<>();for(Object v:MusicStore.values(MusicStore.array(result,"items"))){JSONObject a=(JSONObject)v;JSONArray images=MusicStore.array(a,"images");fields.add(a.optString("id"));fields.add(a.optString("name"));fields.add(a.optString("album_type"));fields.add(a.optString("release_date"));fields.add(images.length()>0?images.optJSONObject(0).optString("url"):"");if(fields.size()>=250)break;}TextPlaylistNative.lookupAlbums(lookup,fields.toArray(new String[0]));}
        else{JSONArray items=kind.equals("albumTracks")?MusicStore.array(result,"items"):MusicStore.array(MusicStore.object(result,"tracks"),"items");ArrayList<String> fields=new ArrayList<>();ArrayList<JSONObject> maps=new ArrayList<>();
            for(Object v:MusicStore.values(items)){JSONObject item=(JSONObject)v;if(!item.optString("uri").startsWith("spotify:track:")||maps.size()>=60)continue;JSONObject albumObject=MusicStore.object(item,"album");ArrayList<String> names=new ArrayList<>(),ids=new ArrayList<>();
                for(Object a:MusicStore.values(MusicStore.array(item,"artists"))){names.add(((JSONObject)a).optString("name"));ids.add(((JSONObject)a).optString("id"));}
                fields.add(item.optString("id"));fields.add(item.optString("uri"));fields.add(item.optString("name"));fields.add(albumObject.length()>0?albumObject.optString("name"):album[1]);fields.add(albumObject.length()>0?albumObject.optString("album_type"):album[2]);fields.add(albumObject.length()>0?albumObject.optString("release_date"):album[3]);
                fields.add(join(names));fields.add(join(ids));fields.add(String.valueOf(item.optInt("popularity",-1)));fields.add(String.valueOf(item.optInt("duration_ms",0)));fields.add(item.has("is_playable")?(item.optBoolean("is_playable")?"1":"0"):"-1");
                fields.add(MusicStore.object(item,"restrictions").optString("reason"));fields.add(MusicStore.object(item,"linked_from").optString("uri"));
                JSONObject t=MusicStore.spotifyTrack(item);if(t.optString("album").isEmpty())MusicStore.put(t,"album",album[1]);if(t.optString("image").isEmpty()&&!album[4].isEmpty())MusicStore.put(t,"image",album[4]);MusicStore.put(t,"playable",!"0".equals(fields.get(fields.size()-3)));maps.add(t);}
            int[] map=TextPlaylistNative.lookupTracks(lookup,fields.toArray(new String[0]));if(map!=null)for(int i=0;i<map.length;i++)if(map[i]==pool.length())pool.put(maps.get(i));}
        if(TextPlaylistNative.lookupFinished(lookup)){completeRow();return;}
        // Transient failure: the native lookup asks for a backoff before repeating the same request (+ jitter).
        int backoff=TextPlaylistNative.lookupBackoff(lookup);int id=lookupId,serial=generation;
        main.postDelayed(()->{if(!closed&&generation==serial&&lookupId==id)step();},fixture?0:backoff>0?backoff+(long)(Math.random()*250):120);
    }
    private void applyDecision(Row row){
        String[] d=TextPlaylistNative.lookupDecision(lookup);if(d==null||d.length<5)return;row.match=d[0];row.reason=d[4];JSONArray ordered=new JSONArray();
        for(int i=5;i+5<d.length;i+=6){JSONObject source=pool.optJSONObject(Integer.parseInt(d[i]));if(source==null)continue;try{JSONObject c=new JSONObject(source.toString());c.put("match",d[i+1]).put("matchLabel",d[i+2]).put("safe",d[i+3].equals("1")).put("reason",d[i+4]).put("requestedArtist",d[i+5].equals("1"));ordered.put(c);}catch(JSONException ignored){}}
        row.lookup=row.match.equals("LOOKUP_ERROR")?"error":"done";
        if(row.manual){int found=-1;for(int i=0;i<ordered.length()&&!row.manualUri.isEmpty();i++)if(row.manualUri.equals(ordered.optJSONObject(i).optString("uri"))){found=i;break;}if(found<0&&row.manualTrack!=null){ordered.put(row.manualTrack);found=ordered.length()-1;}row.choice=found;}
        else{row.choice=Integer.parseInt(d[2]);row.selected=d[3].equals("1");}
        row.candidates=ordered;refresh(row);
    }
    private void completeRow(){
        if(lookup==0)return;int index=indexOf(lookupId);Row row=index>=0?rows.get(index):null;if(row!=null)applyDecision(row);
        String[] resolved=TextPlaylistNative.lookupResolvedArtists(lookup);if(row!=null&&resolved!=null&&resolved.length>0&&resolved[0].equals("1")){String key=TextPlaylistNative.normalize(TextPlaylistNative.utf8(row.artist));if(!key.isEmpty())artistCache.put(key,Arrays.copyOfRange(resolved,1,resolved.length));}
        freeLookup();
        if(!stopReason.isEmpty()){finishSearch("Busca pausada: "+stopReason+" As referências restantes continuam pendentes.");return;}
        if(onlyId>=0){finishSearch("");return;}
        updateReviewCard(index);int serial=generation;final int nextIndex=index+1;main.postDelayed(()->{if(!closed&&busy&&!syncing&&generation==serial)nextRow(nextIndex);},fixture?0:120);
    }
    private void details(){LinearLayout content=ui.column();content.addView(ui.title("A sua identidade",23));
        ArrayList<String> destinations=new ArrayList<>();destinations.add("Criar uma nova playlist");int chosen=0;for(int i=0;i<targets.size();i++){JSONObject target=targets.get(i);destinations.add(target.optString("name"));if(target.optString("id").equals(targetId))chosen=i+1;}destinationInput=spinner(destinations.toArray(new String[0]),chosen);destinationInput.setTag("import-target-playlist");content.addView(destinationInput);content.addView(ui.text("Ao escolher uma playlist existente, as músicas serão acrescentadas ao final, sem substituir o conteúdo atual. Uma cópia local das adições fica neste aparelho.",13,Ui.ACCENT_SOFT));
        nameInput=field("Nome da playlist",name,100,false);descriptionInput=field("Descrição (opcional)",description,300,true);descriptionInput.setMinLines(3);content.addView(nameInput);content.addView(descriptionInput);
        ImageView image=new ImageView(activity);image.setScaleType(ImageView.ScaleType.FIT_CENTER);if(!cover.isEmpty()){byte[] bytes=Base64.decode(cover.substring(cover.indexOf(',')+1),Base64.DEFAULT);image.setImageBitmap(BitmapFactory.decodeByteArray(bytes,0,bytes.length));}else image.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));content.addView(image,new LinearLayout.LayoutParams(-1,ui.dp(180)));
        LinearLayout choices=ui.row();ui.weight(choices,action("Escolher capa…",()->{collectDetails();pendingCover=new WeakReference<>(this);Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("image/*").addCategory(Intent.CATEGORY_OPENABLE);activity.startActivityForResult(i,COVER_REQUEST);}));ui.weight(choices,action("Remover capa",()->{collectDetails();cover="";render();}));content.addView(choices);deduplicateInput=new CheckBox(activity);deduplicateInput.setText("Remover músicas repetidas");deduplicateInput.setTextColor(Ui.TEXT);deduplicateInput.setChecked(deduplicate);content.addView(deduplicateInput);content.addView(ui.text("A playlist é salva neste aparelho primeiro. Você escolhe depois se deseja copiá-la para sua conta Spotify.",14,Ui.MUTED));scroll(content);
    }
    private void collectDetails(){if(step==2&&nameInput!=null){name=nameInput.getText().toString().trim();description=descriptionInput.getText().toString();deduplicate=deduplicateInput.isChecked();int target=destinationInput==null?0:destinationInput.getSelectedItemPosition();targetId=target>0&&target-1<targets.size()?targets.get(target-1).optString("id"):"";}}
    private void saveLocal(){collectDetails();JSONArray selected=new JSONArray();HashSet<String> seen=new HashSet<>();JSONArray audit=new JSONArray();for(Row r:rows)if(r.selected&&r.choice>=0){JSONObject t=r.candidates.optJSONObject(r.choice);if(t!=null&&(!deduplicate||seen.add(t.optString("uri")))){selected.put(t);JSONObject entry=new JSONObject();MusicStore.put(entry,"requested",(r.artist+" - "+r.title));MusicStore.put(entry,"uri",t.optString("uri"));MusicStore.put(entry,"match",t.optString("match"));MusicStore.put(entry,"reason",(r.manual?"Escolha manual: ":"")+t.optString("reason"));MusicStore.put(entry,"manual",r.manual);audit.put(entry);}}try{saved=store.importPlaylist(name,description,cover,selected,audit);if(!targetId.isEmpty())patch("spotifyId",targetId,"appendMode",true,"appendBase",new JSONArray(),"appendBaseReady",false,"syncedCount",0,"cloudComplete",false,"cloudUncertain",false,"coverSynced",true);step=3;busy=true;state("Confirmando gravação local…");render();int serial=generation;store.persist(ok->{if(!closed&&generation==serial)finish(ok?"Playlist salva no Tonalify. Deseja salvar também na conta Spotify?":"A gravação local falhou. Confira o espaço em disco e exporte backup antes de enviar.");});}catch(Exception e){state(e.getMessage());}}
    private JSONObject saved(){return store.playlist(saved);}
    private void patch(Object... values){JSONObject p=new JSONObject();for(int i=0;i<values.length;i+=2)MusicStore.put(p,String.valueOf(values[i]),values[i+1]);store.patchPlaylist(saved,p);}
    private void complete(){JSONObject p=saved();LinearLayout content=ui.column();content.addView(ui.title(p.optString("name"),25));content.addView(ui.text(p.optString("description"),15,Ui.MUTED));content.addView(ui.text(MusicStore.array(p,"tracks").length()+" músicas • salvas neste aparelho",15,Ui.BLUE));content.addView(action("Ouvir / abrir playlist local",()->{dialog.dismiss();MusicPanel.showPlaylist(activity,api,saved);}));ui.space(content,20);content.addView(ui.title(p.optBoolean("appendMode")?"Adicionar à playlist Spotify existente":"Salvar também no Spotify?",22));visibilityInput=spinner(new String[]{"Privada","Pública"},p.optBoolean("spotifyPublic")?1:0);visibilityInput.setEnabled(!busy&&p.optString("spotifyId").isEmpty());visibilityInput.setVisibility(p.optBoolean("appendMode")?View.GONE:View.VISIBLE);content.addView(visibilityInput);content.addView(action(p.optBoolean("appendMode")?"Adicionar músicas à playlist":p.optString("spotifyId").isEmpty()?"Salvar também no Spotify":"Concluir / conferir envio",()->sync(visibilityInput.getSelectedItemPosition()==1)));
        if(message.contains("403")||message.contains("401")||message.contains("Autorize"))content.addView(action("Autorizar permissões…",()->activity.authorizeTextPlaylist()));
        if(p.optBoolean("cloudUncertain")&&p.optString("spotifyId").isEmpty())content.addView(action("Já conferi a conta; permitir nova criação…",()->new AlertDialog.Builder(activity).setTitle("Conferiu sua Biblioteca Spotify?").setMessage("A resposta pode ter se perdido após a criação. Só continue se conferiu a conta e deseja permitir uma nova playlist.").setNegativeButton("Cancelar",null).setPositiveButton("Conferi",(d,w)->{patch("cloudUncertain",false);state("Verificação confirmada. Solicite uma nova cópia se desejar.");render();}).show()));
        content.addView(ui.text("O envio é uma cópia. Sua coleção local permanece salva se a rede ou a API falhar. A capa exige permissão de imagens na conta.",13,Ui.MUTED));scroll(content);
    }
    private void call(String method,String path,String payload,String mime,java.util.function.Consumer<JSONObject> result,java.util.function.Consumer<String> failure){int serial=generation;Runnable send=()->{if(closed||generation!=serial)return;if(fixture){main.postDelayed(()->{if(!closed&&generation==serial)fixtureCall(method,path,payload,mime,result,failure);},30);return;}api.importCall(method,path,payload,mime,result,failure,()->!closed&&generation==serial);};if(method.equals("GET"))send.run();else store.persist(ok->{if(closed||generation!=serial)return;if(ok)send.run();else finish("Não foi possível confirmar a gravação local. O envio foi interrompido. Confira o espaço em disco e faça backup.");});}
    private JSONObject fixtureTrack(String artist,String title)throws Exception{return new JSONObject().put("uri","spotify:track:"+String.format(Locale.ROOT,"%022d",Math.abs((long)(artist+"|"+title).hashCode()))).put("name",title).put("artists",new JSONArray().put(new JSONObject().put("name",artist))).put("album",new JSONObject().put("name","Coleção de teste nativo"));}
    private JSONObject fixtureLookup(String kind,String path,Row row){try{
        if(kind.equals("artists"))return new JSONObject().put("artists",new JSONObject().put("items",new JSONArray().put(new JSONObject().put("id","FIXTUREARTIST00000000"+(Math.abs(row.artist.hashCode())%10)).put("name",row.artist))));
        if(kind.equals("albums")||kind.equals("albumTracks"))return new JSONObject().put("items",new JSONArray());
        if(activity.getIntent().getBooleanExtra("textPlaylistMissing",false)&&row.title.equals("Shove"))return new JSONObject().put("tracks",new JSONObject().put("items",new JSONArray()));
        if(activity.getIntent().getBooleanExtra("textPlaylistUncertain",false)&&row.title.equals("Música 2"))return new JSONObject().put("tracks",new JSONObject().put("items",new JSONArray().put(fixtureTrack(row.artist,"Música 2 - Live"))));
        return new JSONObject().put("tracks",new JSONObject().put("items",new JSONArray().put(fixtureTrack("Karaoke Tribute",row.title).put("popularity",99)).put(fixtureTrack(row.artist,row.title))));
    }catch(Exception impossible){throw new AssertionError(impossible);}}
    private void fixtureCall(String method,String path,String payload,String mime,java.util.function.Consumer<JSONObject> result,java.util.function.Consumer<String> failure){try{
        JSONObject out=new JSONObject();if(method.equals("POST")&&path.equals("me/playlists")){fixtureCreates++;fixtureWrites++;fixturePublic=new JSONObject(payload).optBoolean("public");out.put("id","abcdefghijklmnopqrstuv");}
        else if(method.equals("GET")){JSONArray items=new JSONArray();for(String uri:fixtureRemote)items.put(new JSONObject().put("item",new JSONObject().put("uri",uri)));out.put("items",items);}
        else if(path.endsWith("/items")){fixtureWrites++;if(method.equals("PUT")){fixturePuts++;fixtureRemote.clear();}for(Object uri:MusicStore.values(new JSONObject(payload).getJSONArray("uris")))fixtureRemote.add(String.valueOf(uri));if(activity.getIntent().getBooleanExtra("textPlaylistDropBatch",false)&&!fixtureDropped){fixtureDropped=true;failure.accept("Spotify HTTP 503: resposta de teste perdida");fixtureReceipt();return;}}
        else if(path.endsWith("/images")){fixtureWrites++;fixtureMime=mime;if(payload.length()>256*1024)throw new IOException("fixture cover limit");}
        result.accept(out);fixtureReceipt();
    }catch(Exception error){failure.accept(error.getMessage());}}
    private void fixtureReceipt(){if(!fixture)return;try{JSONObject report=new JSONObject().put("fixture",true).put("native_cpp",true).put("real_spotify_writes",0).put("local_id",saved).put("local_tracks",MusicStore.array(saved(),"tracks").length()).put("description",saved().optString("description")).put("has_cover",!saved().optString("cover").isEmpty()).put("cloud_creates",fixtureCreates).put("cloud_puts",fixturePuts).put("append_mode",saved().optBoolean("appendMode")).put("append_base",MusicStore.array(saved(),"appendBase").length()).put("cloud_writes",fixtureWrites).put("cloud_items",fixtureRemote.size()).put("cover_mime",fixtureMime).put("public",fixturePublic).put("uncertain",saved().optBoolean("cloudUncertain")).put("message",message);java.nio.file.Files.write(new File(activity.getExternalFilesDir(null),"text-playlist-tests.json").toPath(),report.toString(2).getBytes(java.nio.charset.StandardCharsets.UTF_8));}catch(Exception ignored){}}

    private void finish(String text){busy=syncing=false;state(text);render();fixtureReceipt();}
    private void sync(boolean publicPlaylist){if(busy||saved.isEmpty())return;JSONObject p=saved();upload=store.resolve(MusicStore.array(p,"tracks"));if(upload.length()==0)return;cloudId=p.optString("spotifyId");cursor=Math.max(0,Math.min(upload.length(),p.optInt("syncedCount")));++generation;busy=syncing=true;state("Copiando para o Spotify…");render();
        if(!cloudId.isEmpty()){if(!cloudId.matches("[A-Za-z0-9]{22}")){finish("Identificação da playlist Spotify inválida");return;}verifyPages=0;if(p.optBoolean("appendMode")||p.optBoolean("cloudUncertain"))verify("playlists/"+cloudId+"/items?limit=50",new ArrayList<>());else batch();return;}
        if(p.optBoolean("cloudUncertain")){finish("O envio anterior não confirmou a criação. Confira sua Biblioteca Spotify antes de criar outra cópia.");return;}
        JSONObject body=new JSONObject();MusicStore.put(body,"name",p.optString("name"));MusicStore.put(body,"description",p.optString("description"));MusicStore.put(body,"public",publicPlaylist);patch("cloudUncertain",true,"spotifyPublic",publicPlaylist);
        call("POST","me/playlists",body.toString(),"application/json",result->{String id=result.optString("id");if(!id.matches("[A-Za-z0-9]{22}")){finish("O Spotify não confirmou a identificação da playlist. Confira a conta antes de repetir.");return;}cloudId=id;cursor=0;patch("spotifyId",id,"syncedCount",0,"cloudUncertain",false);batch();},error->{if(error.matches("(?s).*HTTP 4[0-9]{2}.*"))patch("cloudUncertain",false);finish(error+". Sua playlist local continua salva.");});
    }
    private void verify(String path,ArrayList<String> remote){if(++verifyPages>12){finish("Não foi possível conferir a paginação. Nenhuma faixa extra foi enviada.");return;}call("GET",path,null,null,result->{for(Object v:MusicStore.values(MusicStore.array(result,"items"))){JSONObject item=(JSONObject)v,t=item.optJSONObject("item");if(t==null)t=item.optJSONObject("track");remote.add(t==null?"":t.optString("uri"));}
        JSONObject state=saved();boolean append=state.optBoolean("appendMode");if(!append&&remote.size()>upload.length()){finish("A playlist Spotify foi alterada. Confira seu conteúdo antes de continuar.");return;}
        String next=result.optString("next");if(!next.isEmpty()&&!next.equals("null")){verify(next,remote);return;}
        if(append){
            ArrayList<String> base=new ArrayList<>();for(Object value:MusicStore.values(MusicStore.array(state,"appendBase")))base.add(String.valueOf(value));
            if(!state.optBoolean("appendBaseReady")){cursor=0;patch("appendBase",new JSONArray(remote),"appendBaseReady",true,"syncedCount",0,"cloudUncertain",false);batch();return;}
            if(remote.size()<base.size()||remote.size()>base.size()+upload.length()){finish("A playlist Spotify mudou desde o início da adição. Confira seu conteúdo antes de continuar.");return;}
            for(int i=0;i<base.size();i++)if(!remote.get(i).equals(base.get(i))){finish("A parte original da playlist Spotify mudou. Nenhuma faixa extra foi enviada.");return;}
            int received=remote.size()-base.size();for(int i=0;i<received;i++)if(!remote.get(base.size()+i).equals(upload.optJSONObject(i).optString("uri"))){finish("As adições confirmadas no Spotify divergem desta retomada. Confira a playlist antes de continuar.");return;}
            cursor=received;patch("syncedCount",cursor,"cloudUncertain",false);batch();return;
        }
        for(int i=0;i<remote.size();i++)if(!remote.get(i).equals(upload.optJSONObject(i).optString("uri"))){finish("A ordem no Spotify mudou. Nenhuma faixa extra foi enviada.");return;}cursor=remote.size();patch("syncedCount",cursor,"cloudUncertain",false);batch();},this::finish);}
    private void batch(){if(cursor>=upload.length()){patch("cloudComplete",true);uploadCover();return;}JSONArray uris=new JSONArray();for(int i=cursor;i<Math.min(cursor+100,upload.length());i++)uris.put(upload.optJSONObject(i).optString("uri"));JSONObject body=new JSONObject();MusicStore.put(body,"uris",uris);int end=cursor+uris.length();patch("cloudUncertain",true);state("Spotify • enviando "+(cursor+1)+"–"+end+" / "+upload.length());call(saved().optBoolean("appendMode")||cursor>0?"POST":"PUT","playlists/"+cloudId+"/items",body.toString(),"application/json",r->{cursor=end;patch("syncedCount",cursor,"cloudUncertain",false);batch();},error->finish(error+". Sua playlist local está salva. Na próxima tentativa, o conteúdo será conferido antes de continuar."));}
    private void uploadCover(){if(saved().optBoolean("appendMode")){finish("Músicas adicionadas à playlist Spotify existente. A cópia local das adições continua salva.");return;}if(cover.isEmpty()||saved().optBoolean("coverSynced")){finish("Playlist salva no Tonalify e no Spotify.");return;}call("PUT","playlists/"+cloudId+"/images",cover.substring(cover.indexOf(',')+1),"image/jpeg",r->{patch("coverSynced",true);finish("Playlist e capa salvas no Tonalify e no Spotify.");},error->finish("Músicas copiadas. Capa pendente: "+error+". Autorize imagens e conclua o envio novamente."));}
    static void photoResult(Activity a,int request,int result,Intent data){if(request!=COVER_REQUEST||result!=Activity.RESULT_OK||data==null||data.getData()==null)return;TextPlaylistWizard w=pendingCover.get();if(w==null||w.closed||w.activity!=a)return;Uri uri=data.getData();w.busy=true;w.state("Preparando capa…");int serial=w.generation;w.api.worker.execute(()->{try{
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=a.getContentResolver().openInputStream(uri)){byte[] block=new byte[8192];int n;while((n=in.read(block))!=-1){if(bytes.size()+n>16*1024*1024)throw new IOException("Imagem maior que 16 MiB");bytes.write(block,0,n);}}byte[] source=bytes.toByteArray();BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(source,0,source.length,o);if(o.outWidth<=0||o.outHeight<=0||(long)o.outWidth*o.outHeight>16000000)throw new IOException("Escolha uma imagem com até 16 megapixels");o.inSampleSize=1;while(o.outWidth/o.inSampleSize>512||o.outHeight/o.inSampleSize>512)o.inSampleSize*=2;o.inJustDecodeBounds=false;Bitmap bitmap=BitmapFactory.decodeByteArray(source,0,source.length,o);if(bitmap==null)throw new IOException("Não foi possível ler a imagem");ByteArrayOutputStream jpeg=new ByteArrayOutputStream();bitmap.compress(Bitmap.CompressFormat.JPEG,80,jpeg);bitmap.recycle();String encoded=Base64.encodeToString(jpeg.toByteArray(),Base64.NO_WRAP);if(encoded.length()>256*1024)throw new IOException("Capa muito grande; escolha uma imagem menor");a.runOnUiThread(()->{if(!w.closed&&w.generation==serial){w.cover="data:image/jpeg;base64,"+encoded;w.busy=false;w.state("Capa pronta");w.render();}});
    }catch(Exception error){a.runOnUiThread(()->{if(!w.closed){w.busy=false;w.state(error.getMessage());w.render();}});}});}
}
