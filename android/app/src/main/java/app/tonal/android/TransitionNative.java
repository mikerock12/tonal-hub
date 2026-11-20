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
    static native boolean arm(long handle,int effect,long tailFrames);
    static native void reset(long handle);
    static native void endCapture(long handle);
    static native int state(long handle);
    static native int push(long handle,float[] samples,int offset,int count);
    static native float[] pull(long handle,int frames,boolean pad);
    static native long buffered(long handle);
    static native long[] plan(int seconds,long durationMs,long remainingMs,long bufferedMs);
    static native int pickRandom(int previous,int random);
    private TransitionNative(){}
}
