package app.tonal.android;

import android.app.AlertDialog;
import android.content.*;
import android.media.AudioDeviceInfo;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.util.*;
import java.util.function.*;

/** Same DSP controls as desktop, laid out for touch; changes are live and saved per output. */
final class AudioPanel {
    private final android.app.Activity activity;private final Consumer<String> notice;private final Runnable reconnect;private final boolean spotify;private final Ui ui;private final AudioOutput audio=NativeBridge.audio;
    private final String key;private AudioConfig config;private LinearLayout content;private AlertDialog dialog;
    private final Handler handler=new Handler(Looper.getMainLooper());private TextView diagnostic;private boolean building;
    private AudioPanel(android.app.Activity a,Consumer<String> n,Runnable r,boolean s){activity=a;notice=n;reconnect=r;spotify=s;ui=new Ui(a);key=audio.routeKey;config=AudioConfig.load(audio.prefs(),key);}
    static void show(MainActivity activity){new AudioPanel(activity,activity::notice,activity::reconnectAudio,true).show();}
    static void show(IndieActivity activity){new AudioPanel(activity,activity::notice,()->{},false).show();}
    private void show(){
        ScrollView scroll=new ScrollView(activity);content=ui.column();ui.pad(content,18,4);scroll.addView(content);build();
        dialog=new AlertDialog.Builder(activity).setTitle("Áudio e saída Bluetooth").setView(scroll).setNeutralButton("Restaurar",null).setPositiveButton("Concluir",null).create();
        dialog.setOnDismissListener(d->handler.removeCallbacks(refresh));dialog.show();dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(v->{config=new AudioConfig();save();build();});handler.postDelayed(refresh,2000);
    }
    private final Runnable refresh=new Runnable(){public void run(){if(dialog==null||!dialog.isShowing())return;if(!key.equals(audio.routeKey)){dialog.dismiss();notice.accept("Saída alterada: o perfil correspondente foi carregado. Reabra Áudio para ajustá-lo.");return;}diagnostic.setText(audio.diagnostics());handler.postDelayed(this,2000);}};
    private void save(){if(!building)audio.updateConfig(config,key);}
    private void title(String text){TextView v=ui.title(text,18);ui.pad(v,0,12);content.addView(v);}
    private void note(String text){TextView v=ui.text(text,12,Ui.MUTED);ui.pad(v,0,6);content.addView(v);}
    private void check(String text,boolean value,Consumer<Boolean> change){CheckBox box=new CheckBox(activity);box.setText(text);box.setTextColor(Ui.TEXT);box.setChecked(value);content.addView(box);box.setOnCheckedChangeListener((v,on)->{change.accept(on);save();});}
    private void choice(String name,String[] values,int selected,IntConsumer change){
        note(name);Spinner spinner=new Spinner(activity);spinner.setContentDescription(name);spinner.setAdapter(new ArrayAdapter<>(activity,android.R.layout.simple_spinner_dropdown_item,values));spinner.setSelection(selected,false);content.addView(spinner);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){int previous=selected;public void onItemSelected(AdapterView<?> p,View v,int i,long id){if(previous==i)return;previous=i;if(!building){change.accept(i);save();}}public void onNothingSelected(AdapterView<?> p){}});
    }
    private void slider(String name,float value,float min,float max,float step,Consumer<Float> change){
        TextView label=ui.text(valueLabel(name,value),13,Ui.MUTED);content.addView(label);SeekBar bar=new SeekBar(activity);bar.setContentDescription(name);bar.setMax(Math.round((max-min)/step));bar.setProgress(Math.round((value-min)/step));content.addView(bar,new LinearLayout.LayoutParams(-1,ui.dp(40)));
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar v,int n,boolean user){if(user){float gain=min+n*step;label.setText(valueLabel(name,gain));change.accept(gain);save();}}public void onStartTrackingTouch(SeekBar v){}public void onStopTrackingTouch(SeekBar v){}});
    }
    private static String valueLabel(String name,float value){return name+String.format(Locale.ROOT," · %.1f %s",value,name.contains("LUFS")?"LUFS":"dB");}
    private void build(){
        building=true;content.removeAllViews();
        title("Saída e perfil");diagnostic=ui.text(audio.diagnostics(),12,Ui.MUTED);content.addView(diagnostic);
        content.addView(ui.action("Escolher saída de mídia",this::outputs));
        content.addView(ui.action("Configurações Bluetooth do Android",()->{try{activity.startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS));}catch(ActivityNotFoundException e){notice.accept("Configurações Bluetooth indisponíveis neste dispositivo.");}}));
        choice("Perfil deste dispositivo",new String[]{"Automático / estéreo","Fones / headphones","Rádio de carro","Caixa de som"},config.profile,i->{config.profile=i;});
        choice("Buffer e estabilidade",new String[]{"Equilibrado • adaptação automática","Priorizar estabilidade","Menor buffer • pode falhar mais"},config.buffer,i->{config.buffer=i;});
        note("O buffer aumenta se houver falhas. Reduzi-lo não elimina o atraso do Bluetooth. As preferências ficam salvas para esta saída; carro e caixa de som usam estéreo.");
        if(spotify){title("Streaming Spotify");SharedPreferences prefs=audio.prefs();int quality=prefs.getInt("quality",320);
        choice("Qualidade Spotify",new String[]{"320 kbps • muito alta","160 kbps • alta","96 kbps • economia"},quality==96?2:quality==160?1:0,i->{prefs.edit().putInt("quality",new int[]{320,160,96}[i]).apply();notice.accept("Qualidade salva. Reconecte o motor Spotify para aplicar.");});
        check("Normalizar volume no Spotify",prefs.getBoolean("normalize",true),on->{prefs.edit().putBoolean("normalize",on).apply();notice.accept("Normalização salva. Reconecte o motor Spotify para aplicar.");});
        note("O volume é controlado pelo celular e pelo receptor. A normalização usa ganho constante por faixa, sem ajuste dinâmico. Para preservar a dinâmica, deixe o compressor desligado.");
        note("A música é baixada progressivamente para atravessar quedas breves de internet. Se a reserva acabar, o Tonal aguarda e tenta retomar. Faixas novas e o login precisam de conexão.");
        content.addView(ui.action("Aplicar streaming e reconectar",reconnect));
        }
        title("Efeitos");check("Ativar processamento de áudio",config.enabled,on->config.enabled=on);
        choice("Preset",AudioConfig.PRESETS,config.preset,i->{config.preset(i);save();build();});
        check("Ativar equalizador de 10 bandas",config.eq,on->config.eq=on);
        for(int i=0;i<10;i++){final int band=i;slider(Dsp.FREQUENCIES[i]+" Hz",config.gains[i],-12,12,.5f,v->{config.gains[band]=v;});}
        content.addView(ui.action("Zerar equalizador",()->{config.gains=new float[10];config.eq=false;config.preset=0;save();build();}));
        slider("Graves • shelf 110 Hz",config.bass,0,12,.5f,v->config.bass=v);
        check("Compressor",config.compressor,on->config.compressor=on);
        title("Normalização de arquivos locais");check("Normalizar por ReplayGain / EBU R128",config.normalization,on->config.normalization=on);
        slider("Alvo LUFS",config.target,-24,-10,.5f,v->config.target=v);
        slider("Teto de amostras",config.ceiling,-6,-.1f,.1f,v->config.ceiling=v);
        note("ReplayGain tem prioridade; sem metadados, o arquivo é analisado em LUFS. O teto protege picos de amostra dos efeitos; não garante true peak após a codificação Bluetooth. Ganhos altos podem reduzir a dinâmica.");
        title("Áudio espacial");
        choice("Renderização espacial",new String[]{"Original","HRTF para fones • OpenAL Soft","IAMF binaural • arquivo IAMF"},audio.binauralSource?2:config.spatial,i->{
            if(i==2){notice.accept(audio.binauralSource?"Esta faixa já é renderizada em IAMF binaural.":"Abra um arquivo .iamf compatível para usar o renderer binaural.");}
            else {config.spatial=i;if(i==1&&config.profile==0)config.profile=1;save();}
            build();
        });
        note(audio.binauralSource?"IAMF binaural ativo • libiamf/OAR. HRTF adicional desativada para esta faixa.":"IAMF binaural disponível para arquivos .iamf compatíveis. O renderer é ativado ao abrir esse conteúdo. Uma música estéreo do Spotify não contém IAMF.");
        note("HRTF virtualiza mono, estéreo, quad, 5.1 e 7.1 em fones, inclusive Bluetooth. Escolha o perfil Fones para utilizá-la. Não há rastreamento de cabeça. O modo Original e o bypass preservam o estéreo sem efeitos.");
        title("Componentes de áudio");note("FFmpeg • OpenAL Soft • libiamf / OAR • libebur128. Processamento local, sem root, microfone ou modo de chamada. Licenças e fontes: pacote local que acompanha este APK.");
        building=false;
    }
    private void outputs(){
        ArrayList<AudioDeviceInfo> devices=new ArrayList<>();ArrayList<String> names=new ArrayList<>();names.add("Automática • Android");
        for(AudioDeviceInfo d:audio.devices()){if(d.getType()==AudioDeviceInfo.TYPE_BLUETOOTH_SCO||d.getType()==AudioDeviceInfo.TYPE_TELEPHONY||d.getType()==AudioDeviceInfo.TYPE_BUILTIN_EARPIECE)continue;devices.add(d);names.add(d.getProductName()+" • "+d.getType());}
        new AlertDialog.Builder(activity).setTitle("Saída solicitada").setItems(names.toArray(new String[0]),(d,i)->{boolean accepted=audio.prefer(i==0?null:devices.get(i-1));notice.accept(accepted?"Saída solicitada; o Android confirma a rota durante a reprodução.":"O Android recusou esta saída de mídia.");}).setNegativeButton("Fechar",null).show();
    }
}
