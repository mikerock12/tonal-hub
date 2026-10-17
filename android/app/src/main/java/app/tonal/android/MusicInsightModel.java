package app.tonal.android;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
/** Quantized musical-theme classifier with identical C++ inference on both platforms. */
final class MusicInsightModel {
    static final String[] LABELS={"amor","saudade","perda","rebeldia","esperanca","celebracao","tensao","introspeccao"};
    private static final boolean READY;
    static{boolean loaded=false;try{System.loadLibrary("tonal_muse");loaded=true;}catch(UnsatisfiedLinkError ignored){}READY=loaded;}
    private static native float[] predict(byte[] normalized);
    static JSONObject analyze(String text){JSONObject out=new JSONObject();if(!READY)return out;
        float[] values=predict(IndieCatalog.normalize(text.substring(0,Math.min(12000,text.length()))).getBytes(StandardCharsets.UTF_8));
        if(values!=null)for(int i=0;i<Math.min(values.length,LABELS.length);i++)MusicStore.put(out,LABELS[i],values[i]);return out;
    }
}
