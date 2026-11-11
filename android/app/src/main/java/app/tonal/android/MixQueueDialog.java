package app.tonal.android;

import android.animation.ValueAnimator;
import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;

final class MixQueueDialog {
    private static final int ROW_DP = 62;
    private final MainActivity host;
    private final Ui ui;
    private final MusicStore store;
    private final ListeningMix mixer;
    private final Artwork artwork;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ArrayList<Entry> entries = new ArrayList<>();
    private final ArrayList<View> rowViews = new ArrayList<>();
    private final ImageButton[] buttons = new ImageButton[3];

    private Dialog dialog;
    private LinearLayout rows, editActions, modeActions;
    private ScrollView scroll;
    private TextView title, summary, status, enrichmentStatus, clear, edit, up, down, remove, apply;
    private JSONObject current;
    private ArrayList<JSONObject> prepared;
    private boolean random, discovery, editing, preparing;
    private int serial, snapshotRevision, visibleRows = 100;
    private int dragFrom = -1, dragTo = -1;
    private float pressY, pointerY;
    private int pressScroll;

    private static final class Entry {
        final JSONObject track;
        boolean marked;
        Entry(JSONObject track){this.track=track;}
    }

    private final Runnable refreshUi = new Runnable() {
        @Override public void run(){
            if(!dialog.isShowing())return;
            if(current!=null&&snapshotRevision!=host.queueRevision()&&dragFrom<0)
                load(3);
            enrichmentStatus.setText(store.discoveryLyrics().status());
            updateActions();
            handler.postDelayed(this,1000);
        }
    };

    private final Runnable scrollDrag = new Runnable() {
        @Override public void run(){
            if(dragFrom<0)return;
            int[] location=new int[2];
            scroll.getLocationOnScreen(location);
            float y=pointerY-location[1];
            int edge=ui.dp(45);
            int step=y<edge?-ui.dp(7):y>scroll.getHeight()-edge?ui.dp(7):0;
            if(step!=0){
                scroll.scrollBy(0,step);
                updateDrag();
            }
            scroll.postOnAnimation(this);
        }
    };

    MixQueueDialog(MainActivity host,SpotifyClient api,MusicStore store){
        this.host=host;
        this.store=store;
        ui=new Ui(host);
        mixer=new ListeningMix(api,store);
        artwork=new Artwork(host);
    }

    void show(){show(3);}

    void show(int mode){
        dialog=new Dialog(host,android.R.style.Theme_Material_NoActionBar);
        LinearLayout root=ui.column();
        root.setBackgroundColor(Ui.BG);
        ui.pad(root,16,10);
        Ui.insets(root);
        LinearLayout heading=ui.row();
        heading.setGravity(Gravity.CENTER_VERTICAL);
        title=ui.title("Fila",24);
        ui.weight(heading,title);
        clear=ui.pill("Limpar",false,this::confirmClear);
        edit=ui.pill("Editar",false,this::toggleEditing);
        heading.addView(clear);
        heading.addView(edit);
        heading.addView(ui.icon("close","Fechar fila",()->dialog.dismiss()),new LinearLayout.LayoutParams(ui.dp(40),ui.dp(48)));
        root.addView(heading);
        summary=ui.text("",13,Ui.MUTED);
        summary.setTag("queue-summary");
        root.addView(summary);
        status=ui.text("",13,Ui.MUTED);
        root.addView(status);

        scroll=new ScrollView(host);
        rows=ui.column();
        scroll.addView(rows);
        root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));

        editActions=ui.row();
        up=ui.pill("↑ Subir",false,()->moveMarked(-1));
        down=ui.pill("↓ Descer",false,()->moveMarked(1));
        remove=ui.pill("Excluir",false,this::removeMarked);
        editActions.addView(up);
        editActions.addView(down);
        editActions.addView(remove);
        root.addView(editActions);

        modeActions=ui.column();
        LinearLayout modes=ui.row();
        String[] icons={"shake","random","discover"};
        String[] labels={"Sortear outra ordem da fila","Sortear reprodução aleatória","Ativar ou desativar Descoberta 2+1"};
        for(int i=0;i<buttons.length;i++){
            final int action=i;
            buttons[i]=ui.icon(icons[i],labels[i],()->generate(action));
            buttons[i].setOnLongClickListener(v->{
                if(action==1)random=false;
                else if(action==2)discovery=false;
                generate(-1);
                return true;
            });
            modes.addView(buttons[i],new LinearLayout.LayoutParams(ui.dp(52),ui.dp(48)));
        }
        modes.addView(ui.icon("close","Descartar prévia",()->load(3)),new LinearLayout.LayoutParams(ui.dp(48),ui.dp(48)));
        modes.getChildAt(3).setVisibility(View.GONE);
        modeActions.addView(modes);
        CheckBox analyze=new CheckBox(host);
        analyze.setText("Analisar letras das candidatas");
        analyze.setTextColor(Ui.TEXT);
        analyze.setChecked(store.discoveryLyrics().enabled());
        analyze.setOnCheckedChangeListener((button,on)->store.discoveryLyrics().setEnabled(on));
        modeActions.addView(analyze);
        modeActions.addView(ui.text("Envia título, artista, álbum e duração ao LRCLIB. Guarda apenas os temas.",11,Ui.MUTED));
        enrichmentStatus=ui.text(store.discoveryLyrics().status(),11,Ui.MUTED);
        modeActions.addView(enrichmentStatus);
        apply=ui.action("Aplicar e reproduzir fila",()->{
            if(prepared!=null&&snapshotRevision==host.queueRevision()){
                host.applyPreparedQueue(prepared,prepared.get(0).optString("uri"));
                dialog.dismiss();
            }else load(3);
        });
        modeActions.addView(apply);
        root.addView(modeActions);

        dialog.setContentView(root);
        dialog.setOnDismissListener(d->{
            serial++;
            dragFrom=-1;
            mixer.cancel();
            artwork.close();
            handler.removeCallbacksAndMessages(null);
            scroll.removeCallbacks(scrollDrag);
        });
        dialog.show();
        dialog.getWindow().setLayout(-1,-1);
        load(mode);
        handler.postDelayed(refreshUi,1000);
    }

    private void load(int mode){
        int request=++serial;
        mixer.cancel();
        prepared=null;
        preparing=false;
        current=null;
        entries.clear();
        editing=false;
        showStatus("Carregando a fila…");
        render();
        host.queueSnapshot(tracks->{
            if(!dialog.isShowing()||request!=serial)return;
            snapshotRevision=host.queueRevision();
            if(!tracks.isEmpty()){
                current=tracks.get(0);
                for(int i=1;i<tracks.size();i++)entries.add(new Entry(tracks.get(i)));
                random=current.optBoolean("_mixRandomEnabled");
                discovery=current.optBoolean("_mixDiscoveryEnabled");
                if(!current.has("_mixDiscoveryEnabled"))
                    for(Entry entry:entries)discovery|=entry.track.optBoolean("_mixExternal");
            }
            showStatus(entries.isEmpty()?"Nenhuma próxima música.":"");
            render();
            if(mode<3)generate(mode);
        },error->{
            if(dialog.isShowing()&&request==serial)showStatus("Não foi possível consultar a fila. "+error);
        });
    }

    private ArrayList<JSONObject> tracks(List<Entry> order){
        ArrayList<JSONObject> tracks=new ArrayList<>(order.size()+1);
        tracks.add(current);
        for(Entry entry:order)tracks.add(entry.track);
        return tracks;
    }

    private void generate(int mode){
        if(current==null||editing||dragFrom>=0)return;
        if(mode==1)random=!random;
        if(mode==2)discovery=!discovery;
        int request=++serial;
        ArrayList<JSONObject> input=prepared==null?tracks(entries):new ArrayList<>(prepared);
        if(mode==0&&input.size()>2){
            ArrayList<JSONObject> rest=new ArrayList<>(input.subList(1,input.size()));
            input.subList(1,input.size()).clear();
            for(int index:ListeningMixNative.order(rest.size(),new java.security.SecureRandom().nextInt()))
                input.add(rest.get(index));
        }
        preparing=true;
        updateActions();
        mixer.prepare(input,0,random,discovery,false,result->{
            if(!dialog.isShowing()||request!=serial)return;
            preparing=false;
            prepared=result;
            render();
        },message->{if(dialog.isShowing()&&request==serial)showStatus(message);});
    }

    private boolean canEdit(){
        return current!=null&&prepared==null&&!preparing&&entries.size()<500&&
                snapshotRevision==host.queueRevision()&&host.canEditQueue();
    }

    private void toggleEditing(){
        if(!canEdit())return;
        editing=!editing;
        if(!editing)for(Entry entry:entries)entry.marked=false;
        render();
    }

    private void confirmClear(){
        if(!canEdit())return;
        final int revision=snapshotRevision;
        new AlertDialog.Builder(host).setTitle("Limpar fila?")
                .setMessage("Remove todas as próximas músicas.")
                .setNegativeButton("Cancelar",null)
                .setPositiveButton("Limpar",(d,w)->{
                    if(revision==snapshotRevision)save(new ArrayList<>());
                }).show();
    }

    private void moveMarked(int step){
        ArrayList<Entry> order=new ArrayList<>(entries);
        boolean moved=false;
        for(int i=step<0?1:order.size()-2;i>=0&&i<order.size();i-=step){
            if(order.get(i).marked&&!order.get(i+step).marked){
                Collections.swap(order,i,i+step);
                moved=true;
            }
        }
        if(moved)save(order);
    }

    private void removeMarked(){
        ArrayList<Entry> order=new ArrayList<>(entries);
        order.removeIf(entry->entry.marked);
        save(order);
    }

    private void save(ArrayList<Entry> order){
        if(!canEdit()||!host.editQueue(tracks(order),snapshotRevision)){
            if(snapshotRevision!=host.queueRevision())load(3);
            showStatus("A fila mudou ou não está disponível para edição.");
            return;
        }
        snapshotRevision=host.queueRevision();
        animateOrder(order);
        entries.clear();
        entries.addAll(order);
        if(entries.isEmpty())editing=false;
        showStatus(entries.isEmpty()?"Nenhuma próxima música.":"");
        render();
    }

    private void animateOrder(ArrayList<Entry> order){
        if(!ValueAnimator.areAnimatorsEnabled()||order.isEmpty())return;
        IdentityHashMap<Entry,Float> offsets=new IdentityHashMap<>();
        for(int i=0;i<rowViews.size();i++){
            Entry entry=entries.get(i);
            offsets.put(entry,i*ui.dp(ROW_DP)+rowViews.get(i).getTranslationY());
        }
        final int revision=snapshotRevision;
        rows.post(()->{
            if(revision!=snapshotRevision||dragFrom>=0||!dialog.isShowing())return;
            for(int i=0;i<rowViews.size()&&i<order.size();i++){
                Float previous=offsets.get(order.get(i));
                if(previous==null)continue;
                View row=rowViews.get(i);
                row.setTranslationY(previous-i*ui.dp(ROW_DP));
                row.animate().translationY(0).setDuration(145).start();
            }
        });
    }

    private void showStatus(String text){
        status.setText(text);
        status.setVisibility(text.isEmpty()?View.GONE:View.VISIBLE);
    }

    private static String summary(List<JSONObject> tracks){
        HashSet<String> albums=new HashSet<>(),artists=new HashSet<>();
        long ms=0;
        boolean missing=false;
        for(JSONObject track:tracks){
            JSONArray credits=track.optJSONArray("artists");
            if(credits!=null)for(int i=0;i<credits.length();i++){
                JSONObject credit=credits.optJSONObject(i);
                String name=credit==null?"":credit.optString("name").trim().toLowerCase(Locale.ROOT);
                if(!name.isEmpty())artists.add(name);
            }
            JSONObject album=track.optJSONObject("album");
            if(album!=null){
                String name=album.optString("name").trim().toLowerCase(Locale.ROOT);
                String id=album.optString("id");
                if(!id.isEmpty())albums.add(id);
                else if(!name.isEmpty())albums.add(name+"|"+CatalogScreen.artists(track).toLowerCase(Locale.ROOT));
            }
            long duration=track.optLong("duration_ms");
            if(duration>0)ms+=duration;
            else missing=true;
        }
        long sec=ms/1000,min=sec/60;
        String time=min>=60?(min/60)+"h "+(min%60)+"min":min+"min "+(sec%60)+"s";
        if(missing)time=ms>0?"≥ "+time:"—";
        return tracks.size()+" mús. · "+albums.size()+" álb. · "+artists.size()+" art. · "+time;
    }

    private void updateActions(){
        boolean preview=prepared!=null||preparing;
        title.setText(editing?"Editar fila":preview?"Prévia da fila":"Fila");
        edit.setText(editing?"Concluir":"Editar");
        editActions.setVisibility(editing?View.VISIBLE:View.GONE);
        modeActions.setVisibility(editing?View.GONE:View.VISIBLE);
        boolean enabled=canEdit()&&!entries.isEmpty()&&dragFrom<0;
        clear.setVisibility(preview?View.GONE:View.VISIBLE);
        edit.setVisibility(preview?View.GONE:View.VISIBLE);
        enabled(clear,enabled);
        enabled(edit,enabled);
        boolean marked=false,canUp=false,canDown=false;
        for(int i=0;i<entries.size();i++)if(entries.get(i).marked){
            marked=true;
            canUp|=i>0&&!entries.get(i-1).marked;
            canDown|=i+1<entries.size()&&!entries.get(i+1).marked;
        }
        enabled(up,enabled&&canUp);
        enabled(down,enabled&&canDown);
        enabled(remove,enabled&&marked);
        apply.setVisibility(prepared==null?View.GONE:View.VISIBLE);
        apply.setEnabled(!preparing);
        ((LinearLayout)buttons[0].getParent()).getChildAt(3).setVisibility(preview?View.VISIBLE:View.GONE);
        buttons[1].setImageDrawable(new Ui.Symbol("random",random?Ui.BLUE:Ui.TEXT));
        buttons[2].setImageDrawable(new Ui.Symbol("discover",discovery?Ui.BLUE:Ui.TEXT));
    }

    private static void enabled(View action,boolean value){
        action.setEnabled(value);
        action.setAlpha(value?1f:0.45f);
    }

    private void render(){
        rows.removeAllViews();
        rowViews.clear();
        List<JSONObject> next=new ArrayList<>();
        if(prepared!=null)next=prepared.subList(1,prepared.size());
        else for(Entry entry:entries)next.add(entry.track);
        summary.setText(summary(next));
        if(current!=null)rows.addView(trackRow(current,-1));
        for(int i=0;i<Math.min(visibleRows,next.size());i++){
            View row=trackRow(next.get(i),i);
            rows.addView(row);
            rowViews.add(row);
        }
        if(next.size()>visibleRows)
            rows.addView(ui.action("Mostrar mais músicas",()->{visibleRows+=100;render();}));
        updateActions();
    }

    private LinearLayout trackRow(JSONObject track,int index){
        LinearLayout row=ui.row();
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setLayoutParams(new LinearLayout.LayoutParams(-1,ui.dp(ROW_DP)));
        row.setBackgroundColor(Ui.BG);
        boolean active=index<0;
        if(editing&&!active){
            Entry entry=entries.get(index);
            CheckBox mark=new CheckBox(host);
            mark.setChecked(entry.marked);
            mark.setContentDescription("Marcar "+track.optString("name"));
            mark.setOnCheckedChangeListener((v,checked)->{entry.marked=checked;updateActions();});
            row.addView(mark,new LinearLayout.LayoutParams(ui.dp(42),-1));
        }
        ImageView cover=new ImageView(host);
        cover.setScaleType(ImageView.ScaleType.CENTER_CROP);
        artwork.load(cover,Artwork.url(track));
        LinearLayout.LayoutParams coverSize=new LinearLayout.LayoutParams(ui.dp(42),ui.dp(42));
        coverSize.rightMargin=ui.dp(10);
        row.addView(cover,coverSize);
        LinearLayout labels=ui.column();
        TextView name=ui.text((active?"Agora · ":"")+track.optString("name"),15,active?Ui.BLUE:Ui.TEXT);
        name.setSingleLine();
        name.setEllipsize(TextUtils.TruncateAt.END);
        if(active)name.setTypeface(null,Typeface.BOLD);
        labels.addView(name);
        TextView artist=ui.text(CatalogScreen.artists(track)+(track.optBoolean("_mixExternal")?" · Descoberta":""),12,Ui.MUTED);
        artist.setSingleLine();
        artist.setEllipsize(TextUtils.TruncateAt.END);
        labels.addView(artist);
        ui.weight(row,labels);
        if(!active&&canEdit()&&entries.size()>1){
            TextView handle=ui.text("≡",25,Ui.MUTED);
            handle.setGravity(Gravity.CENTER);
            handle.setContentDescription("Arrastar "+track.optString("name"));
            handle.setOnTouchListener((v,event)->drag(row,index,event));
            row.addView(handle,new LinearLayout.LayoutParams(ui.dp(44),-1));
        }
        return row;
    }

    private boolean drag(View row,int index,MotionEvent event){
        switch(event.getActionMasked()){
            case MotionEvent.ACTION_DOWN:
                if(!canEdit())return false;
                pressY=pointerY=event.getRawY();
                pressScroll=scroll.getScrollY();
                scroll.requestDisallowInterceptTouchEvent(true);
                return true;
            case MotionEvent.ACTION_MOVE:
                pointerY=event.getRawY();
                if(dragFrom<0){
                    if(Math.abs(pointerY-pressY)<ViewConfiguration.get(host).getScaledTouchSlop())return true;
                    dragFrom=dragTo=index;
                    row.setTranslationZ(ui.dp(8));
                    row.setBackgroundColor(Ui.CARD);
                    scroll.postOnAnimation(scrollDrag);
                    updateActions();
                }
                updateDrag();
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                scroll.requestDisallowInterceptTouchEvent(false);
                scroll.removeCallbacks(scrollDrag);
                int from=dragFrom,to=dragTo;
                dragFrom=dragTo=-1;
                if(from>=0&&to!=from&&event.getActionMasked()==MotionEvent.ACTION_UP){
                    ArrayList<Entry> order=new ArrayList<>(entries);
                    order.add(to,order.remove(from));
                    save(order);
                }else resetDragViews();
                updateActions();
                return true;
            default:return true;
        }
    }

    private void resetDragViews(){
        for(View view:rowViews){
            view.animate().cancel();
            view.setTranslationY(0);
            view.setTranslationZ(0);
            view.setBackgroundColor(Ui.BG);
        }
    }

    private void updateDrag(){
        int height=ui.dp(ROW_DP);
        float offset=pointerY-pressY+scroll.getScrollY()-pressScroll;
        offset=Math.max(-dragFrom*height,Math.min((rowViews.size()-1-dragFrom)*height,offset));
        rowViews.get(dragFrom).setTranslationY(offset);
        int target=Math.max(0,Math.min(rowViews.size()-1,dragFrom+Math.round(offset/height)));
        if(target==dragTo)return;
        dragTo=target;
        for(int i=0;i<rowViews.size();i++){
            if(i==dragFrom)continue;
            float translation=i>dragFrom&&i<=target?-height:i<dragFrom&&i>=target?height:0;
            View row=rowViews.get(i);
            row.animate().cancel();
            if(ValueAnimator.areAnimatorsEnabled())row.animate().translationY(translation).setDuration(130).start();
            else row.setTranslationY(translation);
        }
    }
}
