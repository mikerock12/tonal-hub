package app.tonal.android;

/** Shared desktop audio components, compiled for Android; no privileged Bluetooth APIs. */
final class NativeAudio {
    static { System.loadLibrary("tonal_audio"); }
    static native long create(int rate,int channels,boolean binaural);
    static native void destroy(long handle);
    static native void configure(long handle,float[] settings);
    static native float[] process(long handle,float[] samples);
    static native String prepare(String input,String output,boolean iamf,long generation);
    static native void cancel(long generation);
    private NativeAudio(){}
}
