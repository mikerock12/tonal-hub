package app.tonal.android;
import java.nio.charset.StandardCharsets;
/** JNI facade only. Parsing, artist identity, version policy and the progressive lookup execute in shared C++. */
final class TextPlaylistNative {
    static { System.loadLibrary("tonal_muse"); }
    static byte[] utf8(String s){return (s==null?"":s).getBytes(StandardCharsets.UTF_8);}
    static native String[] parse(byte[] text,int format,byte[] defaultArtist);
    static native String normalize(byte[] value);
    static native String label(byte[] code);
    // One native lookup per reference, used only on the UI thread and always freed.
    static native long lookupCreate(byte[] artist,byte[] title,byte[] original);
    static native void lookupFree(long handle);
    static native String[] lookupNext(long handle);
    static native int[] lookupTracks(long handle,String[] fields);
    static native void lookupArtists(long handle,String[] idNamePairs,boolean seed);
    static native void lookupAlbums(long handle,String[] fields);
    static native void lookupFail(long handle,int status,byte[] message);
    static native boolean lookupFinished(long handle);
    static native int lookupRequests(long handle);
    static native int lookupBackoff(long handle);
    static native String[] lookupDecision(long handle);
    static native String[] lookupResolvedArtists(long handle);
}
