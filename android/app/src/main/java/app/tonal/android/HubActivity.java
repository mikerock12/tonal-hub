package app.tonal.android;

import android.app.Activity;
import android.content.*;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;

/** A launcher only: this process never loads the audio libraries or Spotify credentials. */
public final class HubActivity extends Activity {
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);Ui ui=new Ui(this);getWindow().setStatusBarColor(Ui.BG);getWindow().setNavigationBarColor(Ui.BG);
        LinearLayout page=ui.column();page.setBackgroundColor(Ui.BG);page.setGravity(Gravity.CENTER_VERTICAL);ui.pad(page,24,24);
        Ui.insets(page);
        page.addView(ui.title("Tonal Hub",32));ui.space(page,12);page.addView(ui.text("Sua música, em espaços próprios.",16,Ui.MUTED));ui.space(page,28);
        page.addView(ui.action("Tonalify • Spotify e arquivos locais",()->open(this,MainActivity.class,"")));ui.space(page,12);
        page.addView(ui.action("IndieTonal • outras fontes e shows ao vivo",()->open(this,IndieActivity.class,"")));ui.space(page,24);
        page.addView(ui.text("Audius · MusicBrainz · Live Music Archive\nJamendo com client ID · sua biblioteca OpenSubsonic\n\nVersão "+BuildConfig.VERSION_NAME+" • instalação local",13,Ui.MUTED));
        setContentView(page);
    }
    static void open(Activity from,Class<?> target,String query){
        if(target==MainActivity.class)from.sendBroadcast(new Intent("app.tonal.android.PAUSE_INDIE").setPackage(from.getPackageName()));
        if(target==IndieActivity.class)from.sendBroadcast(new Intent("app.tonal.android.PAUSE_TONALIFY").setPackage(from.getPackageName()));
        Intent intent=new Intent(from,target).addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT|Intent.FLAG_ACTIVITY_SINGLE_TOP);
        if(query!=null&&!query.isBlank())intent.putExtra("literalQuery",query.substring(0,Math.min(200,query.length())));
        from.startActivity(intent);
    }
}
