package app.tonal.android;

import android.app.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;

/** Preview is separate from transport. Only Apply replaces playback, preserving its position. */
final class MixQueueDialog {
    private final MainActivity host;private final Ui ui;private final MusicStore store;private final ListeningMix mixer;
    private final ArrayList<JSONObject> source=new ArrayList<>(),prepared=new ArrayList<>();
    private Dialog dialog;private LinearLayout rows;private TextView status,apply;private final ImageButton[] buttons=new ImageButton[3];
    private boolean random,discovery,edited;private int initial=3,version,visibleRows=100;private String anchor="";
    private final android.os.Handler enrichmentUi=new android.os.Handler(android.os.Looper.getMainLooper());
    private TextView enrichmentStatus;
    private final Runnable enrichmentTick=new Runnable(){public void run(){if(dialog!=null&&dialog.isShowing()){String value=store.discoveryLyrics().status();if(!value.contentEquals(enrichmentStatus.getText()))enrichmentStatus.setText(value);enrichmentUi.postDelayed(this,1500);}}};
    MixQueueDialog(MainActivity host,SpotifyClient api,MusicStore store){this.host=host;this.store=store;ui=new Ui(host);mixer=new ListeningMix(api,store);}
    void show(){show(3);}
    void show(int mode){initial=mode;dialog=new Dialog(host,android.R.style.Theme_Material_NoActionBar);LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,16,16);Ui.insets(root);
        LinearLayout heading=ui.row();ui.weight(heading,ui.title("Fila de reprodução",24));heading.addView(ui.icon("close","Fechar fila",()->dialog.dismiss()));root.addView(heading);
        root.addView(ui.text("Agora e próximas músicas. Toque nos modos para preparar outra ordem; Aplicar confirma a nova fila. Descoberta permanece ao embaralhar. Toque novamente nela para desligar.",13,Ui.MUTED));
        LinearLayout modes=ui.row();String[] icons={"shake","random","discover"},labels={"Sortear outra ordem da fila","Sortear reprodução aleatória","Ativar ou desativar Descoberta 2+1"};
        for(int i=0;i<3;i++){final int n=i;buttons[i]=ui.icon(icons[i],labels[i],()->generate(n));buttons[i].setOnLongClickListener(v->{if(n==1)random=false;else if(n==2)discovery=false;generate(-1);return true;});modes.addView(buttons[i],new LinearLayout.LayoutParams(ui.dp(56),ui.dp(52)));}root.addView(modes);
        CheckBox analyze=new CheckBox(host);analyze.setText("Analisar letras das candidatas em segundo plano");analyze.setTextColor(Ui.TEXT);analyze.setChecked(store.discoveryLyrics().enabled());analyze.setOnCheckedChangeListener((button,on)->store.discoveryLyrics().setEnabled(on));root.addView(analyze);
        root.addView(ui.text("Envia título, artista, álbum e duração ao LRCLIB. Guarda apenas os temas localmente; não altera a fila confirmada.",11,Ui.MUTED));enrichmentStatus=ui.text(store.discoveryLyrics().status(),12,Ui.MUTED);root.addView(enrichmentStatus);
        status=ui.text("Carregando a fila…",13,Ui.MUTED);root.addView(status);rows=ui.column();ScrollView scroll=new ScrollView(host);scroll.addView(rows);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        apply=ui.action("Aplicar e reproduzir fila",()->{if(!prepared.isEmpty()){host.applyPreparedQueue(new ArrayList<>(prepared),anchor);dialog.dismiss();}});apply.setVisibility(View.GONE);root.addView(apply);
        dialog.setContentView(root);dialog.setOnDismissListener(d->{version++;mixer.cancel();enrichmentUi.removeCallbacksAndMessages(null);});dialog.show();enrichmentUi.post(enrichmentTick);dialog.getWindow().setLayout(-1,-1);
        host.queueSnapshot(tracks->{if(!dialog.isShowing())return;prepared.addAll(tracks);if(!tracks.isEmpty())anchor=tracks.get(0).optString("uri");for(JSONObject track:tracks){source.add(track);discovery|=track.optBoolean("_mixDiscoveryEnabled")||track.optBoolean("_mixExternal");}if(!tracks.isEmpty()){random=tracks.get(0).optBoolean("_mixRandomEnabled");if(tracks.get(0).has("_mixDiscoveryEnabled"))discovery=tracks.get(0).optBoolean("_mixDiscoveryEnabled");}status.setText("Fila atual • "+tracks.size()+" músicas disponíveis. Filas informadas pelo Spotify podem ser parciais.");render();if(initial<3)generate(initial);},error->{if(dialog.isShowing())status.setText("Não foi possível consultar a fila. "+error);});
    }
    private void generate(int mode){if(source.isEmpty())return;if(mode==1)random=!random;if(mode==2)discovery=!discovery;edited=true;int serial=++version;ArrayList<JSONObject> input=new ArrayList<>(prepared.isEmpty()?source:prepared);
        if(mode==0&&input.size()>2){ArrayList<JSONObject> rest=new ArrayList<>(input.subList(1,input.size()));input.subList(1,input.size()).clear();for(int i:ListeningMixNative.order(rest.size(),new java.security.SecureRandom().nextInt()))input.add(rest.get(i));}
        apply.setEnabled(false);mixer.prepare(input,0,random,discovery,false,result->{if(!dialog.isShowing()||serial!=version)return;prepared.clear();prepared.addAll(result);apply.setVisibility(View.VISIBLE);apply.setEnabled(true);render();},text->{if(dialog.isShowing()&&serial==version)status.setText(text);});
    }
    private void render(){rows.removeAllViews();for(int i=0;i<Math.min(visibleRows,prepared.size());i++){JSONObject t=prepared.get(i);LinearLayout row=ui.column();ui.pad(row,4,10);row.addView(ui.text((i==0?"Agora • ":i+" • ")+t.optString("name"),16,t.optBoolean("_mixExternal")?Ui.BLUE:Ui.TEXT));row.addView(ui.text(CatalogScreen.artists(t)+(t.optBoolean("_mixExternal")?" • Descoberta":""),12,Ui.MUTED));rows.addView(row);}
        if(prepared.size()>visibleRows)rows.addView(ui.action("Mostrar mais músicas",()->{visibleRows+=100;render();}));
        buttons[1].setImageDrawable(new Ui.Symbol("random",random?Ui.BLUE:Ui.TEXT));buttons[2].setImageDrawable(new Ui.Symbol("discover",discovery?Ui.BLUE:Ui.TEXT));
    }
}
