package app.tonal.android;

import android.graphics.Typeface;
import android.view.*;
import android.view.inputmethod.*;
import android.content.Context;
import android.text.TextUtils;
import android.widget.*;
import java.util.*;
import java.util.function.Consumer;
import org.json.*;

/** Spotify catalog routes, kept separate from the native playback/service lifecycle. */
final class CatalogScreen {
    private final MainActivity host;
    private final SpotifyClient api;
    private final Ui ui;
    private final Artwork artwork;
    private final LinearLayout container;
    private LinearLayout content;
    private ScrollView scroll;
    private int generation;
    private TextView albumSummary;private boolean closed;private boolean showStats;private final ArrayList<View> statViews=new ArrayList<>();
    private Route route=new Route("home","","","","");
    private final ArrayDeque<Route> history=new ArrayDeque<>();
    private static final String[] TYPES={"all","track","artist","album","playlist"};
    private static final String[] FILTERS={"Tudo","Músicas","Artistas","Álbuns","Playlists"};
    private static final class Route {
        final String page,id,name,image,query;String filter="all";int y;boolean shaken,random,discovery,touched;int shakeSeed=new Random().nextInt();
        Route(String p,String i,String n,String a,String q){page=p;id=i;name=n;image=a;query=q;}
    }
    CatalogScreen(MainActivity h,SpotifyClient a,Artwork images,LinearLayout body){host=h;api=a;artwork=images;container=body;ui=new Ui(h);showStats=MusicStore.get(h).data.optBoolean("inlineStats");}
    void home(){tab("home");}void search(){tab("search");}void library(){tab("library");}
    void refreshHome(){if(!closed&&route.page.equals("home")){route.y=scroll==null?0:scroll.getScrollY();render();}}
    void search(String literal){history.clear();route=new Route("search","","","",literal.substring(0,Math.min(200,literal.length())));render();}
    String page(){return route.page;}
    private void tab(String page){history.clear();route=new Route(page,"","","",page.equals("search")?route.query:"");if(page.equals("library"))route.filter="playlist";render();}
    private void open(Route next){route.y=scroll==null?0:scroll.getScrollY();history.push(route);route=next;render();}
    boolean back(){if(history.isEmpty()){if(!route.page.equals("home")){home();return true;}return false;}route=history.pop();render();return true;}
    void close(){closed=true;generation++;}
    private void render(){
        host.cancelMix();generation++;statViews.clear();container.removeAllViews();host.selectTab(route.page);scroll=new ScrollView(host);scroll.setFillViewport(true);scroll.setClipToPadding(false);
        content=ui.column();ui.pad(content,16,8);scroll.addView(content);container.addView(scroll,new LinearLayout.LayoutParams(-1,-1));
        if(!history.isEmpty()){LinearLayout row=ui.row();row.addView(ui.icon("back","Voltar",this::back),new LinearLayout.LayoutParams(ui.dp(44),ui.dp(44)));row.addView(ui.line("Sua música",13,Ui.MUTED));content.addView(row);}
        if(route.page.equals("search")){renderSearch();}
        else if(route.page.equals("home")){renderHome();}
        else if(!api.loggedIn()){signedOut();}
        else switch(route.page){case "library":renderLibrary();break;case "artist":renderArtist();break;case "album":case "playlist":case "liked":renderCollection();break;}
        ui.space(content,16);int y=route.y;scroll.post(()->scroll.scrollTo(0,y));
    }
    private void title(String text){content.addView(ui.title(text,27));ui.space(content,16);}
    private void heading(LinearLayout parent,String text){ui.space(parent,18);parent.addView(ui.title(text,21));ui.space(parent,10);}
    private TextView message(LinearLayout parent,String text){TextView v=ui.text(text,14,Ui.MUTED);ui.pad(v,0,12);parent.addView(v);return v;}
    private void signedOut(){title("Sua música no Tonal");message(content,"Entre para buscar músicas, explorar artistas e ouvir sua biblioteca Spotify.");content.addView(ui.action("Entrar com Spotify",host::login));}
    private void request(String path,Consumer<JSONObject> success,Consumer<String> failure){int ticket=generation;api.get(path,json->{if(!closed&&ticket==generation)success.accept(json);},error->{if(!closed&&ticket==generation)failure.accept(error);});}
    private void problem(LinearLayout target,String error,Runnable retry){
        target.removeAllViews();message(target,error.contains("403")?"O Spotify não permitiu este acesso. Atualize a autorização da biblioteca em Conta.":error);
        target.addView(ui.action(error.contains("403")?"Atualizar autorização":"Tentar novamente",error.contains("403")?host::login:retry));
    }
    private LinearLayout section(String name){heading(content,name);LinearLayout group=ui.column();content.addView(group);return group;}
    private ImageView image(String url,int size,boolean circle){ImageView image=new ImageView(host);image.setScaleType(ImageView.ScaleType.CENTER_CROP);ui.rounded(image,Ui.CARD,circle?size/2:6);image.setLayoutParams(new LinearLayout.LayoutParams(ui.dp(size),ui.dp(size)));artwork.load(image,url);return image;}
    private LinearLayout item(String name,String detail,String img,String type,Runnable click){
        LinearLayout row=ui.row();ui.pad(row,0,7);row.setMinimumHeight(ui.dp(70));ImageView cover=image(img,54,type.equals("artist"));row.addView(cover);
        LinearLayout labels=ui.column();ui.pad(labels,12,0);labels.addView(ui.line(name,15,Ui.TEXT));TextView sub=ui.line(detail,12,Ui.MUTED);sub.setPadding(0,ui.dp(5),0,0);labels.addView(sub);ui.weight(row,labels);ui.clickable(row,click);return row;
    }
    private static String id(JSONObject item){return item.optString("id");}
    static String artists(JSONObject track){JSONArray list=track.optJSONArray("artists");StringJoiner names=new StringJoiner(", ");if(list!=null)for(int i=0;i<list.length();i++){JSONObject a=list.optJSONObject(i);if(a!=null)names.add(a.optString("name"));}return names.toString();}
    private static String safe(JSONObject json,String key){String value=json.optString(key,"");return value.equals("null")?"":value;}
    private static String duration(int ms){return String.format(Locale.ROOT,"%d:%02d",ms/60000,(ms/1000)%60);}
    private void openEntity(JSONObject obj,String type){if(id(obj).isEmpty())return;open(new Route(type,id(obj),obj.optString("name"),Artwork.url(obj),""));}
    private void entityRow(LinearLayout parent,JSONObject obj,String type){
        String detail=type.equals("artist")?"Artista":type.equals("playlist")?"Playlist":"Álbum";
        String by=artists(obj);JSONObject owner=obj.optJSONObject("owner");if(type.equals("playlist")&&owner!=null)by=owner.optString("display_name");if(!by.isEmpty())detail+=" • "+by;
        parent.addView(item(obj.optString("name"),detail,Artwork.url(obj),type,()->openEntity(obj,type)));
    }
    private void song(LinearLayout parent,JSONObject track,ArrayList<JSONObject> queue,String context,int index,String fallbackImage){
        if(track.optString("uri").isEmpty())return;
        String art=Artwork.url(track);if(art.isEmpty())art=fallbackImage;
        boolean available=!track.optBoolean("is_local")&&(!track.has("is_playable")||track.optBoolean("is_playable"));
        LinearLayout row=item(track.optString("name"),artists(track)+"  •  "+(available?duration(track.optInt("duration_ms")):"Indisponível"),art,"track",()->{if(!queue.isEmpty()&&(route.shaken||route.random||route.discovery||route.touched))host.playMix(queue,index,route.random,route.discovery,route.page.equals("album"));else host.playTracks(queue,index,context,track);});
        if(!available)row.setAlpha(.45f);
        JSONObject stat=MusicStore.get(host).summary("track",MusicStore.key(MusicStore.spotifyTrack(track)));LinearLayout labels=(LinearLayout)row.getChildAt(1);TextView badge=ui.text(statsText(stat),12,Ui.ACCENT_SOFT);badge.setPadding(0,ui.dp(6),0,0);badge.setVisibility(showStats?View.VISIBLE:View.GONE);statViews.add(badge);labels.addView(badge);ui.clickable(badge,()->MusicPanel.showStatistics(host,api,"track",MusicStore.key(MusicStore.spotifyTrack(track))));
        JSONObject saved=MusicStore.spotifyTrack(track);boolean liked=MusicStore.get(host).liked(saved);ImageButton heart=ui.favorite(liked,()->{});heart.setContentDescription((liked?"Remover das favoritas: ":"Favoritar: ")+track.optString("name"));heart.setOnClickListener(v->toggleFavorite(saved,heart));row.addView(heart,new LinearLayout.LayoutParams(ui.dp(44),ui.dp(48)));
        row.addView(ui.icon("more","Opções de "+track.optString("name"),()->trackMenu(track)),new LinearLayout.LayoutParams(ui.dp(44),ui.dp(48)));parent.addView(row);
    }
    private void toggleFavorite(JSONObject track,ImageButton heart){
        MusicStore store=MusicStore.get(host);boolean on=!store.liked(track);heart.setEnabled(false);
        api.mutate(on?"PUT":"DELETE","me/library?uris="+SpotifyClient.encode(track.optString("uri")),null,r->{if(closed)return;store.favorite(track,on);heart.setEnabled(true);heart.setContentDescription((on?"Remover das favoritas: ":"Favoritar: ")+track.optString("title"));heart.setImageDrawable(new Ui.Symbol(on?"heart_filled":"heart",on?Ui.BLUE:Ui.TEXT));},error->{if(closed)return;heart.setEnabled(true);host.notice(error+". Se faltar permissão, atualize a autorização da biblioteca.");});
    }
    private void trackMenu(JSONObject track){
        ArrayList<String> options=new ArrayList<>();ArrayList<Runnable> actions=new ArrayList<>();
        options.add("Colocar como próxima da fila");actions.add(()->host.insertInQueue(track,true));
        options.add("Colocar como última da fila");actions.add(()->host.insertInQueue(track,false));
        options.add("Favoritos, playlists e letras");actions.add(()->host.trackPersonalMenu(track));
        JSONObject album=track.optJSONObject("album");if(album!=null&&!id(album).isEmpty()){options.add("Abrir álbum");actions.add(()->openEntity(album,"album"));}
        JSONArray artists=track.optJSONArray("artists");if(artists!=null)for(int i=0;i<artists.length();i++){JSONObject a=artists.optJSONObject(i);if(a==null)continue;options.add("Artista: "+a.optString("name"));actions.add(()->openEntity(a,"artist"));}
        if(!options.isEmpty())new android.app.AlertDialog.Builder(host).setTitle(track.optString("name")).setItems(options.toArray(new String[0]),(d,i)->actions.get(i).run()).show();
    }
    private void chips(String[] labels,String[] values,String selected,Consumer<String> change){
        HorizontalScrollView h=new HorizontalScrollView(host);h.setHorizontalScrollBarEnabled(false);LinearLayout row=ui.row();
        for(int i=0;i<labels.length;i++){final String v=values[i];TextView chip=ui.pill(labels[i],selected.equals(v),()->change.accept(v));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-2,-2);p.rightMargin=ui.dp(8);row.addView(chip,p);}h.addView(row);content.addView(h);ui.space(content,12);
    }
    private void renderHome(){
        int hour=Calendar.getInstance().get(Calendar.HOUR_OF_DAY);title(hour<12?"Bom dia":hour<18?"Boa tarde":"Boa noite");
        if(!api.loggedIn()){signedOut();return;}
        LinearLayout shortcuts=ui.column();content.addView(shortcuts);ArrayList<JSONObject> quick=new ArrayList<>();quick.add(new JSONObject());quickTiles(shortcuts,quick);
        request("me/playlists?limit=5",j->{JSONArray list=j.optJSONArray("items");if(list!=null)for(int i=0;i<list.length();i++){JSONObject p=list.optJSONObject(i);if(p!=null)quick.add(p);}quickTiles(shortcuts,quick);},e->{});
        LinearLayout recent=section("Álbuns tocados recentemente"),topAlbums=section("Seus álbuns mais ouvidos"),discovery=section("Novos álbuns para explorar"),saved=section("Seus álbuns");
        int ticket=generation;ArrayList<JSONObject> local=new ArrayList<>(),remote=new ArrayList<>(),serviceRecent=new ArrayList<>();
        Runnable drawRecent=()->{ArrayList<JSONObject> combined=new ArrayList<>(local);Set<String> ids=new HashSet<>();for(JSONObject a:combined)ids.add(id(a));for(JSONObject a:serviceRecent)if(ids.add(id(a)))combined.add(a);albumStrip(recent,combined,12);};
        Runnable draw=()->{if(closed||ticket!=generation)return;ArrayList<JSONObject> combined=new ArrayList<>(local);Set<String> ids=new HashSet<>();for(JSONObject a:combined)ids.add(id(a));for(JSONObject a:remote)if(ids.add(id(a)))combined.add(a);saved.removeAllViews();for(JSONObject a:combined)entityRow(saved,a,"album");if(combined.isEmpty())message(saved,"Os álbuns ouvidos neste aparelho e os salvos na sua conta aparecem aqui.");};
        MusicStore.get(host).requestHomeAlbums(albums->{if(closed||ticket!=generation)return;for(Object v:MusicStore.values(albums))local.add((JSONObject)v);drawRecent.run();ArrayList<JSONObject> ranked=new ArrayList<>(local);ranked.sort(Comparator.comparingLong((JSONObject a)->a.optLong("listenedMs")).reversed());albumStrip(topAlbums,ranked,8);if(MusicStore.get(host).homeHistoryError)message(topAlbums,"Histórico completo indisponível: mostrando apenas escutas recentes deste aparelho.");draw.run();});
        request("me/albums?limit=20",j->{for(Object v:MusicStore.values(MusicStore.array(j,"items"))){JSONObject album=((JSONObject)v).optJSONObject("album");if(album!=null)remote.add(album);}draw.run();},e->draw.run());
        request("me/player/recently-played?limit=50",j->{Set<String> ids=new HashSet<>();for(Object v:MusicStore.values(MusicStore.array(j,"items"))){JSONObject t=((JSONObject)v).optJSONObject("track");if(t==null)t=((JSONObject)v).optJSONObject("item");JSONObject a=t==null?null:t.optJSONObject("album");if(a!=null&&!id(a).isEmpty()&&ids.add(id(a)))serviceRecent.add(a);}drawRecent.run();},e->{});
        LinearLayout top=section("Seus artistas");new Pager(top,"me/top/artists?limit=10&time_range=short_term","artist","",false,"").load();
        message(discovery,"Álbuns dos seus artistas que você ainda não ouviu neste aparelho.");
        request("me/top/artists?limit=3&time_range=short_term",j->{Set<String> seen=new HashSet<>();ArrayList<JSONObject> suggestions=new ArrayList<>();for(Object v:MusicStore.values(MusicStore.array(j,"items"))){JSONObject artist=(JSONObject)v;request("artists/"+id(artist)+"/albums?include_groups=album,single&limit=5",r->{for(Object value:MusicStore.values(MusicStore.array(r,"items"))){JSONObject album=(JSONObject)value;boolean heard=false;for(JSONObject a:local)if(id(a).equals(id(album))){heard=true;break;}if(!heard&&seen.add(id(album)))suggestions.add(album);}albumStrip(discovery,suggestions,12);},e->{});}},e->message(discovery,"As sugestões precisam de acesso ao catálogo Spotify."));
    }
    private void albumStrip(LinearLayout target,List<JSONObject> albums,int limit){
        target.removeAllViews();HorizontalScrollView strip=new HorizontalScrollView(host);strip.setHorizontalScrollBarEnabled(false);LinearLayout cards=ui.row();strip.addView(cards);target.addView(strip);
        for(int i=0;i<Math.min(limit,albums.size());i++){JSONObject album=albums.get(i);LinearLayout card=ui.column();card.addView(image(Artwork.url(album),148,false));ui.space(card,8);TextView name=ui.text(album.optString("name"),14,Ui.TEXT);name.setMaxLines(2);name.setEllipsize(TextUtils.TruncateAt.END);card.addView(name);card.addView(ui.line(artists(album),12,Ui.MUTED));ui.clickable(card,()->openEntity(album,"album"));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(ui.dp(148),-2);p.rightMargin=ui.dp(16);cards.addView(card,p);}
        if(albums.isEmpty())message(target,"Ainda sem álbuns disponíveis nesta seção.");
    }

    private void quickTiles(LinearLayout target,ArrayList<JSONObject> items){target.removeAllViews();LinearLayout row=null;for(int i=0;i<items.size();i++){if(i%2==0){row=ui.row();target.addView(row);}JSONObject obj=items.get(i);boolean liked=i==0;LinearLayout tile=ui.row();ui.rounded(tile,Ui.CARD,5);ImageView art=image(Artwork.url(obj),48,false);if(liked){ui.rounded(art,Ui.BLUE,0);art.setImageDrawable(new Ui.Symbol("heart",Ui.TEXT));art.setPadding(ui.dp(12),ui.dp(12),ui.dp(12),ui.dp(12));}tile.addView(art);TextView name=ui.text(liked?"Músicas Curtidas":obj.optString("name"),12,Ui.TEXT);name.setTypeface(null,Typeface.BOLD);name.setMaxLines(2);name.setEllipsize(TextUtils.TruncateAt.END);ui.pad(name,8,2);ui.weight(tile,name);ui.clickable(tile,()->{if(liked)open(new Route("liked","","Músicas Curtidas","",""));else openEntity(obj,"playlist");});LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,ui.dp(56),1);p.setMargins(0,0,ui.dp(i%2==0?8:0),ui.dp(8));row.addView(tile,p);}if(items.size()%2==1)row.addView(new View(host),new LinearLayout.LayoutParams(0,1,1));}
    private void renderSearch(){
        title("Buscar");LinearLayout input=ui.row();ui.rounded(input,Ui.CARD,24);ImageButton submit=ui.icon("search","Pesquisar",()->{});
        EditText query=new EditText(host);query.setTextColor(Ui.TEXT);query.setHintTextColor(Ui.MUTED);query.setTextSize(15);query.setHint("O que você quer ouvir?");query.setSingleLine(true);query.setBackgroundColor(0x00000000);query.setPadding(ui.dp(16),ui.dp(10),0,ui.dp(10));query.setImeOptions(EditorInfo.IME_ACTION_SEARCH);query.setText(route.query);
        Runnable execute=()->{String q=query.getText().toString().trim();if(q.isEmpty())return;MusicStore.get(host).rememberSearch(q);((InputMethodManager)host.getSystemService(Context.INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(query.getWindowToken(),0);route=new Route("search","","","",q);render();};
        submit.setOnClickListener(v->execute.run());query.setOnEditorActionListener((v,action,event)->{if(action==EditorInfo.IME_ACTION_SEARCH||(event!=null&&event.getKeyCode()==KeyEvent.KEYCODE_ENTER)){execute.run();return true;}return false;});ui.weight(input,query);input.addView(submit,new LinearLayout.LayoutParams(ui.dp(48),ui.dp(48)));content.addView(input);ui.space(content,16);
        chips(FILTERS,TYPES,route.filter,v->{route.filter=v;route.y=0;render();});
        if(!route.query.isEmpty()){String literal=route.query;content.addView(ui.action("Explorar “"+literal+"” no IndieTonal",()->HubActivity.open(host,IndieActivity.class,literal)));message(content,"Outras fontes e shows ao vivo • a consulta digitada será aberta no outro player.");}
        if(route.query.isEmpty()){MusicStore journal=MusicStore.get(host);JSONArray recent=journal.searches();if(recent.length()>0){LinearLayout head=ui.row();ui.weight(head,ui.title("Buscas recentes",20));head.addView(ui.icon("trash","Limpar histórico de buscas",()->{journal.forgetSearch("");render();}));content.addView(head);for(Object value:MusicStore.values(recent)){String text=String.valueOf(value);LinearLayout row=ui.row();row.addView(ui.icon("history","Repetir busca: "+text,()->{query.setText(text);execute.run();}));TextView label=ui.line(text,16,Ui.TEXT);ui.clickable(label,()->{query.setText(text);execute.run();});ui.weight(row,label);row.addView(ui.icon("close","Remover busca: "+text,()->{journal.forgetSearch(text);render();}));content.addView(row);}}}
        if(!api.loggedIn()){signedOut();return;}if(route.query.isEmpty()){message(content,"Encontre músicas, artistas, álbuns e playlists no Spotify.");return;}
        String type=route.filter.equals("all")?"track,artist,album,playlist":route.filter;
        String path="search?type="+type+"&limit=10&q="+SpotifyClient.encode(route.query);
        if(!route.filter.equals("all")){LinearLayout group=ui.column();content.addView(group);new Pager(group,path,route.filter,"",true,"").load();return;}
        LinearLayout all=ui.column();content.addView(all);message(all,"Buscando…");request(path,json->{all.removeAllViews();int count=0;for(int k=1;k<TYPES.length;k++){String kind=TYPES[k];JSONObject group=json.optJSONObject(kind+"s");JSONArray list=group==null?null:group.optJSONArray("items");if(list==null||list.length()==0)continue;heading(all,FILTERS[k]);ArrayList<JSONObject> tracks=new ArrayList<>();for(int i=0;i<list.length();i++){JSONObject item=list.optJSONObject(i);if(item!=null)tracks.add(item);}for(int i=0;i<Math.min(4,tracks.size());i++){JSONObject item=tracks.get(i);if(kind.equals("track"))song(all,item,tracks,"",i,"");else entityRow(all,item,kind);count++;}all.addView(ui.action("Ver "+FILTERS[k].toLowerCase(new Locale("pt","BR")),()->{route.filter=kind;route.y=0;render();}));}if(count==0)message(all,"Nenhum resultado para “"+route.query+"”. Tente outro nome.");},e->problem(all,e,this::render));
    }
    private void renderLibrary(){
        content.addView(ui.action("Criar playlist…",host::createPlaylist));
        title("Sua Biblioteca");chips(new String[]{"Playlists","Álbuns","Artistas"},new String[]{"playlist","album","artist"},route.filter,v->{route.filter=v;route.y=0;render();});
        content.addView(item("Músicas Curtidas","Playlist • Sua coleção","","album",()->open(new Route("liked","","Músicas Curtidas","",""))));
        LinearLayout list=ui.column();content.addView(list);
        String endpoint=route.filter.equals("album")?"me/albums?limit=10":route.filter.equals("artist")?"me/following?type=artist&limit=10":"me/playlists?limit=10";
        new Pager(list,endpoint,route.filter,route.filter.equals("album")?"album":"",false,"").load();
    }
    private static String statsText(JSONObject s){return String.format(new Locale("pt","BR"),"%d escutas · %.1f min · %d músicas",s.optInt("plays"),s.optLong("ms")/60000.,s.optInt("tracks"));}
    private void statsToggle(String kind,String value){LinearLayout row=ui.row();TextView label=ui.text("Suas escutas neste aparelho",13,Ui.MUTED);ui.weight(row,label);ImageButton toggle=ui.icon("chart","Mostrar ou ocultar estatísticas",()->{showStats=!showStats;MusicStore.put(MusicStore.get(host).data,"inlineStats",showStats);MusicStore.get(host).save();for(View v:statViews)v.setVisibility(showStats?View.VISIBLE:View.GONE);});row.addView(toggle);content.addView(row);JSONObject stats=MusicStore.get(host).summary(kind,value);TextView summary=ui.text(statsText(stats),15,Ui.ACCENT_SOFT);ui.pad(summary,12,12);ui.rounded(summary,0xff1d303c,12);summary.setVisibility(showStats?View.VISIBLE:View.GONE);summary.setTag(value);ui.clickable(summary,()->MusicPanel.showStatistics(host,api,kind,String.valueOf(summary.getTag())));statViews.add(summary);if(kind.equals("album"))albumSummary=summary;content.addView(summary);}
    private void renderArtist(){
        ImageView photo=image(route.image,208,true);LinearLayout hero=ui.column();hero.setGravity(Gravity.CENTER_HORIZONTAL);hero.addView(photo);ui.space(hero,20);TextView name=ui.title(route.name,30);name.setGravity(Gravity.CENTER);hero.addView(name,new LinearLayout.LayoutParams(-1,-2));TextView description=ui.text("ARTISTA · SPOTIFY",12,Ui.MUTED);description.setGravity(Gravity.CENTER);hero.addView(description);ui.space(hero,10);TextView genres=ui.text("",14,Ui.ACCENT_SOFT);genres.setGravity(Gravity.CENTER);hero.addView(genres);content.addView(hero);
        request("artists/"+route.id,json->{String url=Artwork.large(json);if(!url.isEmpty())artwork.load(photo,url);JSONObject followers=json.optJSONObject("followers");description.setText(followers!=null?String.format(new Locale("pt","BR"),"ARTISTA · %,d seguidores",followers.optLong("total")):"ARTISTA · SPOTIFY");StringJoiner tags=new StringJoiner("  ·  ");JSONArray list=MusicStore.array(json,"genres");for(int i=0;i<Math.min(4,list.length());i++)tags.add(list.optString(i));genres.setText(tags.toString());},e->{});
        statsToggle("artist",route.name);
        LinearLayout tracks=section("Músicas");String q="artist:\""+route.name.replace("\"","")+"\"";
        Pager songs=new Pager(tracks,"search?type=track&limit=10&q="+SpotifyClient.encode(q),"track","",true,"");songs.artistId=route.id;songs.load();
        for(String group:new String[]{"album","single","compilation"}){LinearLayout albums=section(group.equals("album")?"Álbuns":group.equals("single")?"Singles e EPs":"Coletâneas");new Pager(albums,"artists/"+route.id+"/albums?include_groups="+group+"&limit=10","album","",false,"").load();}
    }
    static JSONObject albumMetadata(JSONObject full){JSONObject small=new JSONObject();for(String key:new String[]{"id","name","images","artists","release_date","total_tracks"})if(full.has(key))MusicStore.put(small,key,full.opt(key));return small;}
    private void renderCollection(){
        LinearLayout hero=ui.column();hero.setGravity(Gravity.CENTER_HORIZONTAL);ImageView cover=image(route.image,216,false);hero.addView(cover);ui.space(hero,20);TextView kind=ui.text(route.page.equals("liked")?"SUA COLEÇÃO":route.page.equals("album")?"ÁLBUM":"PLAYLIST",11,Ui.ACCENT_SOFT);kind.setGravity(Gravity.CENTER);hero.addView(kind);ui.space(hero,6);TextView albumTitle=ui.title(route.name,26);albumTitle.setGravity(Gravity.CENTER);albumTitle.setMaxLines(4);hero.addView(albumTitle,new LinearLayout.LayoutParams(-1,-2));LinearLayout meta=ui.column();ui.space(meta,8);TextView info=ui.text("Carregando detalhes…",14,Ui.MUTED);info.setGravity(Gravity.CENTER);meta.addView(info);
        String context=route.page.equals("liked")?"":"spotify:"+route.page+":"+route.id;
        ImageButton play=ui.icon("play","Reproduzir álbum",()->{});play.setImageDrawable(new Ui.Symbol("play",Ui.ON_ACCENT));ui.rounded(play,Ui.BLUE,28);LinearLayout actions=ui.row();actions.setGravity(Gravity.CENTER);actions.addView(play,new LinearLayout.LayoutParams(ui.dp(56),ui.dp(56)));meta.addView(actions);hero.addView(meta,new LinearLayout.LayoutParams(-1,-2));content.addView(hero);
        if(route.page.equals("album"))statsToggle("album",route.name+"|");
        LinearLayout list=section("Músicas");String endpoint=route.page.equals("liked")?"me/tracks?limit=50":route.page+"s/"+route.id+(route.page.equals("album")?"/tracks?limit=50":"/items?limit=50");
        Pager pager=new Pager(list,endpoint,"track",route.page.equals("liked")?"track":route.page.equals("playlist")?"item":"",false,context);pager.fallbackImage=route.image;if(route.page.equals("album")){pager.albumContext=new JSONObject();MusicStore.put(pager.albumContext,"id",route.id);MusicStore.put(pager.albumContext,"name",route.name);MusicStore.put(pager.albumContext,"images",new JSONArray().put(new JSONObject()));MusicStore.put(MusicStore.array(pager.albumContext,"images").optJSONObject(0),"url",route.image);}play.setOnClickListener(v->pager.play());
        ImageButton shake=ui.icon("shake","Shake: sortear ordem; segure para restaurar",()->{route.touched=true;host.cancelMix();route.shaken=true;route.shakeSeed=new java.security.SecureRandom().nextInt();pager.refreshRows();});ImageButton random=ui.icon("random","Reprodução aleatória",()->{route.touched=true;host.cancelMix();route.random=!route.random;pager.refreshRows();});ImageButton discover=ui.icon("discover","Descoberta 2+1: uma música externa a cada duas",()->{route.touched=true;host.cancelMix();route.discovery=!route.discovery;pager.refreshRows();});
        shake.setOnLongClickListener(v->{route.shaken=false;route.touched=true;host.cancelMix();pager.refreshRows();return true;});
        pager.modeButtons=new ImageButton[]{shake,random,discover};for(ImageButton button:pager.modeButtons)actions.addView(button,new LinearLayout.LayoutParams(ui.dp(52),ui.dp(52)));pager.load();
        if(!route.page.equals("liked"))request(route.page+"s/"+route.id,json->{String img=Artwork.large(json);if(route.page.equals("album")){pager.albumContext=albumMetadata(json);for(JSONObject track:pager.items){MusicStore.put(track,"album",pager.albumContext);MusicStore.get(host).remember(MusicStore.spotifyTrack(track));}}if(!img.isEmpty()){artwork.load(cover,img);pager.fallbackImage=img;}String description=artists(json);if(route.page.equals("playlist")){JSONObject owner=json.optJSONObject("owner");description=owner==null?"Playlist":owner.optString("display_name");}else {String date=json.optString("release_date","");if(date.matches("[0-9]{4}-[0-9]{2}-[0-9]{2}"))try{date=new java.text.SimpleDateFormat("d 'de' MMMM 'de' yyyy",new Locale("pt","BR")).format(new java.text.SimpleDateFormat("yyyy-MM-dd",Locale.ROOT).parse(date));}catch(Exception ignored){}description+="\n"+date+" · "+json.optInt("total_tracks",pager.items.size())+" músicas";}info.setText(description);
            if(route.page.equals("album")&&albumSummary!=null){albumSummary.setTag(artists(json)+"|"+route.name);albumSummary.setText(statsText(MusicStore.get(host).summary("album",artists(json)+"|"+route.name)));}JSONArray artists=json.optJSONArray("artists");if(artists!=null&&artists.length()>0){JSONObject a=artists.optJSONObject(0);if(a!=null){info.setTextColor(Ui.BLUE);info.setOnClickListener(v->openEntity(a,"artist"));}}},e->{});
    }
    /** One pager owns one section and one monotonically growing playback queue. */
    private final class Pager {
        final LinearLayout target;final String kind,wrapper,context;final boolean search;
        final ArrayList<JSONObject> items=new ArrayList<>();final Set<String> seen=new HashSet<>();
        String next,artistId="",fallbackImage="";JSONObject albumContext;boolean busy;int sourceOffset;TextView control;ImageButton[] modeButtons;int visibleLimit=100;final ArrayList<JSONObject> original=new ArrayList<>();boolean capped;
        void play(){if(busy){host.notice("Carregando a seleção…");return;}if(!items.isEmpty()){if(route.shaken||route.random||route.discovery||route.touched)host.playMix(items,route.random?new java.security.SecureRandom().nextInt(items.size()):0,route.random,route.discovery,route.page.equals("album"));else host.playTracks(items,0,context,items.get(0));}else host.notice("Aguarde as músicas carregarem.");}
        void refreshRows(){if(modeButtons==null)return;items.clear();if(route.shaken){for(int i:ListeningMixNative.order(original.size(),route.shakeSeed))items.add(original.get(i));}else items.addAll(original);target.removeAllViews();boolean[] active={route.shaken,route.random,route.discovery};String[] icons={"shake","random","discover"};for(int i=0;i<3;i++){modeButtons[i].setImageDrawable(new Ui.Symbol(icons[i],active[i]?Ui.BLUE:Ui.TEXT));modeButtons[i].setSelected(active[i]);}
            if(route.shaken||route.random||route.discovery||route.touched){message(target,"Ordem temporária • Reproduzir aplica os modos selecionados. A playlist salva permanece na ordem original."+(capped?" Limite: primeiras 500 músicas.":""));}
            for(int i=0;i<Math.min(visibleLimit,items.size());i++)song(target,items.get(i),items,context,i,fallbackImage);if(items.size()>visibleLimit)target.addView(ui.action("Mostrar mais músicas",()->{visibleLimit+=100;refreshRows();}));
            if(!next.isEmpty()&&!capped){control=ui.action("Carregar mais",this::load);target.addView(control);if(route.shaken||route.random||route.discovery||route.touched)load();}}

        Pager(LinearLayout target,String url,String kind,String wrapper,boolean search,String context){this.target=target;next=url;this.kind=kind;this.wrapper=wrapper;this.search=search;this.context=context;}
        void load(){if(busy||next.isEmpty())return;busy=true;if(control!=null)target.removeView(control);control=message(target,"Carregando…");String path=next;
            request(path,json->{busy=false;target.removeView(control);JSONObject group=search?json.optJSONObject(kind+"s"):kind.equals("artist")&&json.optJSONObject("artists")!=null?json.optJSONObject("artists"):json;JSONArray array=group==null?null:group.optJSONArray("items");if(array!=null)for(int i=0;i<array.length();i++){int sourceIndex=sourceOffset++;JSONObject item=array.optJSONObject(i);if(item==null)continue;if(modeButtons!=null&&original.size()>=500){capped=true;break;}if(!wrapper.isEmpty()){JSONObject nested=item.optJSONObject(wrapper);if(nested==null&&kind.equals("track"))nested=item.optJSONObject("track");if(nested==null)nested=item.optJSONObject("item");if(nested==null)continue;item=nested;}if(!artistId.isEmpty()&&!hasArtist(item,artistId))continue;if(albumContext!=null&&!item.has("album"))MusicStore.put(item,"album",albumContext);String key=item.optString("uri",item.optString("id"));if((context.isEmpty()||!kind.equals("track"))&&!seen.add(key))continue;if(!context.isEmpty())try{item.put("_contextIndex",sourceIndex);}catch(JSONException ignored){}int index=items.size();items.add(item);if(modeButtons!=null)original.add(item);if(kind.equals("track"))song(target,item,items,context,index,fallbackImage);else entityRow(target,item,kind);}next=group==null?"":safe(group,"next");if(modeButtons!=null){capped=capped||(original.size()>=500&&!next.isEmpty());if(!capped&&!next.isEmpty()&&(route.shaken||route.random||route.discovery||route.touched)){load();return;}refreshRows();return;}if(items.isEmpty())message(target,"Nenhum item disponível.");if(!next.isEmpty()){control=ui.action("Carregar mais",this::load);target.addView(control);}},error->{busy=false;target.removeView(control);control=ui.action(error.contains("403")?"Atualizar autorização da biblioteca":error+" • Tentar novamente",error.contains("403")?host::login:this::load);target.addView(control);});
        }
    }
    private static boolean hasArtist(JSONObject track,String id){JSONArray list=track.optJSONArray("artists");if(list!=null)for(int i=0;i<list.length();i++){JSONObject a=list.optJSONObject(i);if(a!=null&&id.equals(a.optString("id")))return true;}return false;}
}
