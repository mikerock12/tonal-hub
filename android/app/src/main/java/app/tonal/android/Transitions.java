package app.tonal.android;

import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.graphics.Typeface;

/**
 * Transições entre as músicas da fila (mesmas regras do Windows).
 * O gatilho segue os eventos do motor, não a tela: funciona com o app em
 * segundo plano. Desde a 0.17.3 a posição vem da contagem de PCM desde o
 * marcador de começo de faixa do librespot (exata); os eventos de posição do
 * motor ficam como reserva, por exemplo depois de um seek. O fim de A na
 * mescla também vem do marcador, não desta estimativa.
 */
final class Transitions {
    static volatile boolean enabled;
    static volatile boolean trimSilence;
    static volatile String effect="random";
    static volatile int seconds=6;      // fim da música atual com o efeito
    static volatile int introSeconds=6; // começo da próxima na mescla
    static volatile String status="";
    private static final Handler main=new Handler(Looper.getMainLooper());
    private static String trackUri="",armedUri="";
    // Ocorrência no PCM (sequência de marcadores) já armada e o pedido em curso.
    private static long armedSequence=-1,armedId,shownId;
    private static long durationMs,guardUntil;
    private static int lastEffect=-1;
    private static Runnable scheduled;

    static void load(SharedPreferences prefs){
        enabled=prefs.getBoolean("transitionsEnabled",false);
        trimSilence=prefs.getBoolean("trimSilence",false);
        String chosen=prefs.getString("transitionEffect","random");
        effect=chosen.equals("random")||TransitionNative.index(chosen)>=0?chosen:"random";
        seconds=Math.max(3,Math.min(10,prefs.getInt("transitionSeconds",6)));
        introSeconds=Math.max(3,Math.min(10,prefs.getInt("transitionIntroSeconds",6)));
    }
    static void save(SharedPreferences prefs,boolean on,String chosen,int duration){save(prefs,on,chosen,duration,introSeconds);}
    static void save(SharedPreferences prefs,boolean on,String chosen,int duration,int intro){
        enabled=on;effect=chosen;seconds=Math.max(3,Math.min(10,duration));introSeconds=Math.max(3,Math.min(10,intro));
        prefs.edit().putBoolean("transitionsEnabled",on).putString("transitionEffect",chosen).putInt("transitionSeconds",seconds).putInt("transitionIntroSeconds",introSeconds).putBoolean("trimSilence",trimSilence).apply();
        if(!on&&!trimSilence)cancel();
        status="";
    }
    static boolean active(){return enabled||trimSilence;}
    static String name(String id){if(id.equals("random"))return "Aleatório";int i=TransitionNative.index(id);return i<0?id:TransitionNative.NAMES[i];}

    /** Chamado na thread principal quando o motor carrega uma faixa (TrackChanged). */
    static void onTrack(String uri,int duration){
        // Cada carga é uma ocorrência nova, inclusive a mesma música de novo.
        unschedule();guardUntil=SystemClock.elapsedRealtime()+2500;
        trackUri=uri;durationMs=duration;
    }
    /** Só uma mescla concluída com B audível vira "Última transição". */
    private static void updateResult(){
        if(armedId==0||shownId==armedId)return;
        long[] t=NativeBridge.audio.transitionTrace();
        if(t==null||t[TransitionNative.T_ID]!=armedId)return;
        long phase=t[TransitionNative.T_PHASE];
        if(phase!=TransitionNative.PHASE_COMPLETED&&phase!=TransitionNative.PHASE_ABORTED)return;
        shownId=armedId;
        int effect=(int)t[TransitionNative.T_EFFECT];
        if(phase==TransitionNative.PHASE_COMPLETED&&!TransitionNative.overlapVerified(t)&&(t[TransitionNative.T_TRIMMED_A]>0||t[TransitionNative.T_TRIMMED_B]>0))
            status=String.format(java.util.Locale.ROOT,"Silêncio removido: %.1f s no fim e %.1f s no começo",t[TransitionNative.T_TRIMMED_A]*1000.0/t[TransitionNative.T_RATE],t[TransitionNative.T_TRIMMED_B]*1000.0/t[TransitionNative.T_RATE]);
        else if(phase==TransitionNative.PHASE_COMPLETED&&TransitionNative.overlapVerified(t)&&effect>=0&&effect<TransitionNative.NAMES.length)
            status="Última transição: "+TransitionNative.NAMES[effect];
        else if(t[TransitionNative.T_REASON]==TransitionNative.REASON_NEXT_NOT_PREPARED)
            status="Última troca sem efeito: a próxima música não chegou a tempo";
        else if(t[TransitionNative.T_REASON]==TransitionNative.REASON_NO_EXACT_BOUNDARY)
            status="Última troca sem efeito: o começo da próxima não foi identificado";
    }
    private static long occurrence(){return NativeBridge.audio.markersSeen?NativeBridge.audio.trackSequence:-2;}
    /** Eventos de posição do motor (Playing/PositionChanged/Seeked), na thread principal. */
    static void onProgress(String uri,int position,boolean active,long eventAt){
        updateResult();
        boolean armed=NativeBridge.audio.markersSeen?occurrence()==armedSequence:uri.equals(armedUri);
        if((!enabled&&!trimSilence)||!TransitionNative.READY||!active||!uri.equals(trackUri)||durationMs<=0||armed||scheduled!=null)return;
        if(NativeBridge.repeatMode==RepeatMode.TRACK||!NativeBridge.spotifyEngine)return;
        int state=NativeBridge.audio.transitionState();
        if(state==1||state==2)return;
        long now=SystemClock.elapsedRealtime();
        if(now<guardUntil)return;
        long overlap=enabled?Math.min(Math.max(seconds,introSeconds)*1000L,durationMs/3):0;
        if(enabled&&overlap<1500)return;
        // Posição do PCM já entregue; o som está atrás pelo atraso da anterior.
        long upcoming=remaining(durationMs,position,eventAt);
        long audible=upcoming+NativeBridge.audio.transitionLatencyMs();
        long window=overlap+TransitionNative.PREROLL_MS+TransitionNative.LOAD_MARGIN_MS+500;
        // Os eventos chegam a cada segundo: agenda o momento exato de começar a ler.
        if(audible>window+1300||audible<1000)return;
        final String target=uri;final long track=durationMs;
        scheduled=()->{scheduled=null;arm(target,track,position,eventAt);};
        main.postDelayed(scheduled,Math.max(0,audible-window));
    }
    private static void arm(String uri,long duration,int position,long eventAt){
        synchronized(NativeBridge.intent){if(!NativeBridge.intent.desired)return;}
        boolean armed=NativeBridge.audio.markersSeen?occurrence()==armedSequence:uri.equals(armedUri);
        if((!enabled&&!trimSilence)||!uri.equals(trackUri)||armed||!NativeBridge.playing)return;
        long upcoming=remaining(duration,position,eventAt);
        long audible=upcoming+NativeBridge.audio.transitionLatencyMs();
        long[] plan=enabled?TransitionNative.plan(seconds,introSeconds,duration,audible,Math.max(0,upcoming)):TransitionNative.planSilenceTrim(duration,audible,Math.max(0,upcoming));
        if(plan==null||plan.length<(enabled?4:2)||plan[0]==0)return;
        int chosen=enabled?TransitionNative.index(effect):0;
        if(chosen<0)chosen=TransitionNative.pickRandom(lastEffect<0?TransitionNative.IDS.length:lastEffect,(int)(Math.random()*Integer.MAX_VALUE));
        long id=enabled?NativeBridge.audio.armTransition(chosen,plan[1],plan[2],plan[3],trimSilence):NativeBridge.audio.armTransition(0,plan[1],0,0,true);
        if(id!=0){
            armedUri=uri;armedSequence=occurrence();armedId=id;lastEffect=chosen;
        }else guardUntil=SystemClock.elapsedRealtime()+1000;  // recusado: nova tentativa logo depois
    }
    /** Quanto da faixa o motor ainda não entregou: pela contagem desde o marcador, senão pelo evento. */
    private static long remaining(long duration,int position,long eventAt){
        long exact=NativeBridge.audio.trackPositionMs();
        if(exact>=0&&exact<=duration+2000)return duration-exact;
        return duration-position-(SystemClock.elapsedRealtime()-eventAt);
    }
    private static void unschedule(){if(scheduled!=null){main.removeCallbacks(scheduled);scheduled=null;}}
    /** Pulos, seleções e seeks trocam na hora, sem efeito. */
    static void cancel(){
        main.post(()->{unschedule();guardUntil=SystemClock.elapsedRealtime()+2500;armedUri="";armedSequence=-1;});
        NativeBridge.audio.cancelTransition();
    }
    /** Configurações das transições: ligar, efeito fixo ou aleatório e tempo de mesclagem. */
    static void show(android.app.Activity a,Runnable changed){
        Ui ui=new Ui(a);android.widget.LinearLayout body=ui.column();ui.pad(body,18,12);
        SharedPreferences prefs=NativeBridge.audio.prefs();
        final boolean[] on={enabled};final String[] chosen={effect};final int[] length={seconds};final int[] intro={introSeconds};
        android.widget.Switch toggle=new android.widget.Switch(a);toggle.setText("Mesclar o fim de cada música com o começo da próxima");toggle.setTextColor(Ui.TEXT);toggle.setChecked(on[0]);toggle.setTag("transitions-enabled");
        body.addView(toggle);
        android.widget.Switch silence=new android.widget.Switch(a);silence.setText("Tirar silêncio no final da faixa / começo da próxima");silence.setTextColor(Ui.TEXT);silence.setChecked(trimSilence);silence.setTag("trim-silence-enabled");body.addView(silence);
        silence.setOnCheckedChangeListener((v,checked)->{trimSilence=checked;prefs.edit().putBoolean("trimSilence",checked).apply();cancel();status="";});
        body.addView(ui.text("Remove somente quadros com os dois canais em zero digital. Funciona sozinha ou junto com as transições; volumes baixos não são cortados.",12,Ui.MUTED));
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
        group.setOnCheckedChangeListener((g,checked)->{int i=checked-1000;if(i>=0&&i<ids.length){chosen[0]=ids[i];save(prefs,on[0],chosen[0],length[0],intro[0]);}});
        body.addView(group);
        android.widget.TextView timeLabel=ui.title("Fim da música atual com o efeito: "+length[0]+" s",18);body.addView(timeLabel);
        android.widget.SeekBar time=new android.widget.SeekBar(a);ui.seek(time);time.setMax(7);time.setProgress(length[0]-3);time.setContentDescription("Fim da música atual com o efeito, de 3 a 10 segundos");time.setTag("transition-seconds");
        time.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(android.widget.SeekBar b,int value,boolean user){length[0]=3+value;timeLabel.setText("Fim da música atual com o efeito: "+length[0]+" s");if(user)save(prefs,on[0],chosen[0],length[0],intro[0]);}
            public void onStartTrackingTouch(android.widget.SeekBar b){}public void onStopTrackingTouch(android.widget.SeekBar b){}});
        body.addView(time);
        android.widget.TextView introLabel=ui.title("Começo da próxima na mescla: "+intro[0]+" s",18);body.addView(introLabel);
        android.widget.SeekBar introBar=new android.widget.SeekBar(a);ui.seek(introBar);introBar.setMax(7);introBar.setProgress(intro[0]-3);introBar.setContentDescription("Começo da próxima música na mescla, de 3 a 10 segundos");introBar.setTag("transition-intro-seconds");
        introBar.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(android.widget.SeekBar b,int value,boolean user){intro[0]=3+value;introLabel.setText("Começo da próxima na mescla: "+intro[0]+" s");if(user)save(prefs,on[0],chosen[0],length[0],intro[0]);}
            public void onStartTrackingTouch(android.widget.SeekBar b){}public void onStopTrackingTouch(android.widget.SeekBar b){}});
        body.addView(introBar);
        Runnable enable=()->{group.setEnabled(on[0]);for(int i=0;i<group.getChildCount();i++)group.getChildAt(i).setEnabled(on[0]);time.setEnabled(on[0]);introBar.setEnabled(on[0]);};enable.run();
        toggle.setOnCheckedChangeListener((v,checked)->{on[0]=checked;save(prefs,on[0],chosen[0],length[0],intro[0]);enable.run();});
        body.addView(ui.text("A próxima música começa a tocar por baixo da atual pelo tempo escolhido e as duas terminam a mescla juntas, no último instante da atual. O efeito ocupa o fim da música atual. Pular, escolher outra música ou buscar um trecho troca na hora, sem efeito. Não vale para a repetição de uma música só nem para arquivos locais.",12,Ui.MUTED));
        if(!status.isEmpty())body.addView(ui.text(status,13,Ui.ACCENT_SOFT));
        if(!TransitionNative.READY)body.addView(ui.text("Componente nativo das transições indisponível neste aparelho.",13,0xffffb4a8));
        android.widget.ScrollView scroll=new android.widget.ScrollView(a);scroll.addView(body);
        new android.app.AlertDialog.Builder(a).setTitle("Transições entre músicas").setView(scroll).setPositiveButton("Concluir",null).setOnDismissListener(d->{if(changed!=null)changed.run();}).show();
    }
    private Transitions(){}
}
