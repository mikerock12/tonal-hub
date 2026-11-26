package app.tonal.android;

/** JNI do mixer de transições (src/playback/TransitionMixer.cpp, compartilhado com o Windows). */
final class TransitionNative {
    static final boolean READY;
    static {boolean loaded=false;try{System.loadLibrary("tonal_muse");loaded=true;}catch(UnsatisfiedLinkError ignored){}READY=loaded;}
    static final String[] IDS={"fade","echo","filter","brake","reverb","roll"};
    static final String[] NAMES={"Fade cruzado","Eco de despedida","Troca de graves","Freio de vinil","Banho de reverb","Loop acelerado"};
    static final String[] DESCRIPTIONS={
        "A música atual baixa enquanto a próxima sobe, sem buraco de volume.",
        "A música atual se repete em ecos de um lado para o outro enquanto a próxima entra.",
        "O filtro fecha a música atual; a próxima entra sem graves até assumir a pista.",
        "A música atual desacelera como um toca-discos desligado e a próxima cai em seguida.",
        "A música atual se dissolve num salão de reverb por baixo da próxima.",
        "Trechos cada vez menores da música atual se repetem até a próxima explodir."};
    static int index(String id){for(int i=0;i<IDS.length;i++)if(IDS[i].equals(id))return i;return -1;}
    static native long create(int rate);
    static native void destroy(long handle);
    static native int rate(long handle);
    /** Com `exact` o fim de A é o próximo boundary() (marcador do librespot); a estimativa só posiciona. */
    static native boolean arm(long handle,int effect,long upcomingFrames,long outroFrames,long introFrames,boolean exact,long id,boolean trimSilence);
    /** Motivo do último arm() recusado (TransitionReason do C++). */
    static native int rejection(long handle);
    /** O motor começou outra faixa: o próximo push() já é de B. */
    static native void boundary(long handle);
    static native boolean wantsInput(long handle);
    static native void reset(long handle);
    static native void endCapture(long handle);
    /** A mesma faixa voltou a tocar depois de um endCapture (recuperação de buffering). */
    static native void resumeCapture(long handle);
    static native int state(long handle);
    static native int push(long handle,float[] samples,int offset,int count);
    /** Escreve em `out` (reutilizado); retorna os quadros produzidos. Sem alocação. */
    static native int pullInto(long handle,float[] out,int frames);
    /** Índices de trace(). */
    static final int T_ID=0,T_PHASE=1,T_REASON=2,T_EFFECT=3,T_RATE=4,T_CONF_OUTRO=5,T_CONF_INTRO=6,T_OUTRO=7,T_INTRO=8,T_OVERLAP=9,
        T_BOUNDARY=10,T_PREPARED_B=11,T_WAITED=12,T_MIXED=13,T_B_FRAMES=14,T_MISSING_B=15,T_ENERGY_A=16,T_ENERGY_B=17,T_TRIMMED_A=18,T_TRIMMED_B=19;
    static final int PHASE_IDLE=0,PHASE_ARMED=1,PHASE_MIXING=2,PHASE_COMPLETED=3,PHASE_ABORTED=4;
    static final int REASON_NEXT_NOT_PREPARED=5,REASON_NO_EXACT_BOUNDARY=4,REASON_CANCELLED=6;
    /** O que a última transição realmente fez (fora do caminho do PCM). */
    static native long[] trace(long handle);
    static boolean overlapVerified(long[] t){return t!=null&&t.length>T_ENERGY_B&&t[T_B_FRAMES]>0&&t[T_ENERGY_A]>0&&t[T_ENERGY_B]>0;}
    static native long latency(long handle);
    /** {arm 0/1, upcomingMs, outroMs, introMs}; mesmas regras do Windows. */
    static native long[] plan(int outroSeconds,int introSeconds,long durationMs,long audibleRemainingMs,long upcomingMs);
    static final int PREROLL_MS=2500,LOAD_MARGIN_MS=6000;
    static native long[] planSilenceTrim(long durationMs,long audibleRemainingMs,long upcomingMs);
    static native int pickRandom(int previous,int random);
    private TransitionNative(){}
}
