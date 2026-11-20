package app.tonal.android;

import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.graphics.Typeface;

/**
 * Transições entre as músicas da fila (mesmas regras do Windows).
 * O gatilho segue os eventos do motor, não a tela: funciona com o app em
 * segundo plano. A posição do librespot é a do PCM já entregue ao AudioTrack,
 * então o restante da faixa no fluxo é exatamente duração − posição.
 */
final class Transitions {
    static volatile boolean enabled;
    static volatile String effect="random";
    static volatile int seconds=6;
    static volatile String status="";
    private static final Handler main=new Handler(Looper.getMainLooper());
    private static String trackUri="",armedUri="";
    private static long durationMs,guardUntil;
    private static int lastEffect=-1;
    private static Runnable scheduled;

    static void load(SharedPreferences prefs){
        enabled=prefs.getBoolean("transitionsEnabled",false);
        String chosen=prefs.getString("transitionEffect","random");
        effect=chosen.equals("random")||TransitionNative.index(chosen)>=0?chosen:"random";
        seconds=Math.max(3,Math.min(10,prefs.getInt("transitionSeconds",6)));
    }
    static void save(SharedPreferences prefs,boolean on,String chosen,int duration){
        enabled=on;effect=chosen;seconds=Math.max(3,Math.min(10,duration));
        prefs.edit().putBoolean("transitionsEnabled",on).putString("transitionEffect",chosen).putInt("transitionSeconds",seconds).apply();
        if(!on)cancel();
        status="";
    }
    static String name(String id){if(id.equals("random"))return "Aleatório";int i=TransitionNative.index(id);return i<0?id:TransitionNative.NAMES[i];}

    /** Chamado na thread principal quando o motor carrega uma faixa (TrackChanged). */
    static void onTrack(String uri,int duration){
        if(!uri.equals(trackUri)){unschedule();guardUntil=SystemClock.elapsedRealtime()+2500;}
        trackUri=uri;durationMs=duration;
    }
    /** Eventos de posição do motor (Playing/PositionChanged/Seeked), na thread principal. */
    static void onProgress(String uri,int position,boolean active,long eventAt){
        if(!enabled||!TransitionNative.READY||!active||!uri.equals(trackUri)||durationMs<=0||uri.equals(armedUri)||scheduled!=null)return;
        if(NativeBridge.repeatMode==RepeatMode.TRACK||!NativeBridge.spotifyEngine)return;
        long now=SystemClock.elapsedRealtime();
        if(now<guardUntil)return;
        long overlap=Math.min(seconds*1000L,durationMs/3);
        if(overlap<1500)return;
        long remaining=durationMs-position-(now-eventAt);
        // Os eventos chegam a cada segundo: agenda o início exato da leitura adiantada.
        if(remaining>overlap+1300||remaining<1000)return;
        final String target=uri;final long track=durationMs;
        scheduled=()->{scheduled=null;arm(target,track,position,eventAt);};
        // 400 ms extras cobrem o buffer do AudioTrack enquanto o fim de A é lido.
        main.postDelayed(scheduled,Math.max(0,remaining-overlap-400));
    }
    private static void arm(String uri,long duration,int position,long eventAt){
        synchronized(NativeBridge.intent){if(!NativeBridge.intent.desired)return;}
        if(!enabled||!uri.equals(trackUri)||uri.equals(armedUri)||!NativeBridge.playing)return;
        long remaining=duration-position-(SystemClock.elapsedRealtime()-eventAt);
        if(remaining<1000)return;
        int chosen=TransitionNative.index(effect);
        if(chosen<0)chosen=TransitionNative.pickRandom(lastEffect<0?TransitionNative.IDS.length:lastEffect,(int)(Math.random()*Integer.MAX_VALUE));
        if(NativeBridge.audio.armTransition(chosen,remaining)){
            armedUri=uri;lastEffect=chosen;status="Última transição: "+TransitionNative.NAMES[chosen];
        }
    }
    private static void unschedule(){if(scheduled!=null){main.removeCallbacks(scheduled);scheduled=null;}}
    /** Pulos, seleções e seeks trocam na hora, sem efeito. */
    static void cancel(){
        main.post(()->{unschedule();guardUntil=SystemClock.elapsedRealtime()+2500;armedUri="";});
        NativeBridge.audio.cancelTransition();
    }
    /** Configurações das transições: ligar, efeito fixo ou aleatório e tempo de mesclagem. */
    static void show(android.app.Activity a,Runnable changed){
        Ui ui=new Ui(a);android.widget.LinearLayout body=ui.column();ui.pad(body,18,12);
        SharedPreferences prefs=NativeBridge.audio.prefs();
        final boolean[] on={enabled};final String[] chosen={effect};final int[] length={seconds};
        android.widget.Switch toggle=new android.widget.Switch(a);toggle.setText("Mesclar o fim de cada música com o começo da próxima");toggle.setTextColor(Ui.TEXT);toggle.setChecked(on[0]);toggle.setTag("transitions-enabled");
        body.addView(toggle);
        body.addView(ui.title("Efeito",18));
        android.widget.RadioGroup group=new android.widget.RadioGroup(a);
        String[] ids=new String[TransitionNative.IDS.length+1];ids[0]="random";System.arraycopy(TransitionNative.IDS,0,ids,1,TransitionNative.IDS.length);
        for(int i=0;i<ids.length;i++){
            String id=ids[i];android.widget.RadioButton option=new android.widget.RadioButton(a);option.setId(1000+i);option.setTag("transition-"+id);
            String description=id.equals("random")?"Um efeito diferente a cada troca de música.":TransitionNative.DESCRIPTIONS[i-1];
            android.text.SpannableString label=new android.text.SpannableString(name(id)+"\n"+description);
            label.setSpan(new android.text.style.StyleSpan(Typeface.BOLD),0,name(id).length(),0);
            label.setSpan(new android.text.style.ForegroundColorSpan(Ui.MUTED),name(id).length()+1,label.length(),0);
            label.setSpan(new android.text.style.RelativeSizeSpan(.85f),name(id).length()+1,label.length(),0);
            option.setText(label);option.setTextColor(Ui.TEXT);ui.pad(option,4,6);
            group.addView(option);if(id.equals(chosen[0]))group.check(option.getId());
        }
        group.setOnCheckedChangeListener((g,checked)->{int i=checked-1000;if(i>=0&&i<ids.length){chosen[0]=ids[i];save(prefs,on[0],chosen[0],length[0]);}});
        body.addView(group);
        android.widget.TextView timeLabel=ui.title("Tempo de mesclagem: "+length[0]+" s",18);body.addView(timeLabel);
        android.widget.SeekBar time=new android.widget.SeekBar(a);ui.seek(time);time.setMax(7);time.setProgress(length[0]-3);time.setContentDescription("Tempo de mesclagem de 3 a 10 segundos");time.setTag("transition-seconds");
        time.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(android.widget.SeekBar b,int value,boolean user){length[0]=3+value;timeLabel.setText("Tempo de mesclagem: "+length[0]+" s");if(user)save(prefs,on[0],chosen[0],length[0]);}
            public void onStartTrackingTouch(android.widget.SeekBar b){}public void onStopTrackingTouch(android.widget.SeekBar b){}});
        body.addView(time);
        Runnable enable=()->{group.setEnabled(on[0]);for(int i=0;i<group.getChildCount();i++)group.getChildAt(i).setEnabled(on[0]);time.setEnabled(on[0]);};enable.run();
        toggle.setOnCheckedChangeListener((v,checked)->{on[0]=checked;save(prefs,on[0],chosen[0],length[0]);enable.run();});
        body.addView(ui.text("A mesclagem começa alguns segundos antes do fim da música e a próxima entra por baixo. Pular, escolher outra música ou buscar um trecho troca na hora, sem efeito. Não vale para a repetição de uma música só nem para arquivos locais.",12,Ui.MUTED));
        if(!status.isEmpty())body.addView(ui.text(status,13,Ui.ACCENT_SOFT));
        if(!TransitionNative.READY)body.addView(ui.text("Componente nativo das transições indisponível neste aparelho.",13,0xffffb4a8));
        android.widget.ScrollView scroll=new android.widget.ScrollView(a);scroll.addView(body);
        new android.app.AlertDialog.Builder(a).setTitle("Transições entre músicas").setView(scroll).setPositiveButton("Concluir",null).setOnDismissListener(d->{if(changed!=null)changed.run();}).show();
    }
    private Transitions(){}
}
