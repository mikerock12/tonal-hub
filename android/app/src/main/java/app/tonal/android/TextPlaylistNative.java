package app.tonal.android;
import java.nio.charset.StandardCharsets;
/** JNI facade only. Parsing, queries and ranking all execute in shared C++. */
final class TextPlaylistNative {
    static { System.loadLibrary("tonal_muse"); }
    static byte[] utf8(String s){return s.getBytes(StandardCharsets.UTF_8);}
    static native String[] parse(byte[] text,int format,byte[] defaultArtist);
    static native String[] queries(byte[] artist,byte[] title);
    static native double[] rank(byte[] artist,byte[] title,String[] titles,String[] artists,int[] popularity);
}
