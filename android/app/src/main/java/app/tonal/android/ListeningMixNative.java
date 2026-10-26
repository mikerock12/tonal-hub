package app.tonal.android;
final class ListeningMixNative {
 static {System.loadLibrary("tonal_muse");}
 static native int[] order(int count,int seed);
 static native int[] plan(String[] source,String[] pool,int selected,boolean random,boolean discovery,int seed,boolean album);
 static native String[] queries(String genres,int year);
 static native int[] alignQueue(String[] prepared,String[] observed);
}
