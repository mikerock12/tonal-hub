package app.tonal.android;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import java.io.File;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import org.json.JSONArray;
import org.json.JSONObject;

/**
 * Visual themes shared with the desktop (assets/themes.json is a copy of
 * resources/themes.json). The choice is a small file so the hub and IndieTonal
 * processes read the current value on every start, without a stale cache.
 */
final class AppTheme {
    private static JSONObject catalog;
    private AppTheme(){}

    static synchronized JSONArray themes(Context context){
        if(catalog==null){
            try(InputStream in=context.getAssets().open("themes.json")){catalog=new JSONObject(new String(in.readAllBytes(),StandardCharsets.UTF_8));}
            catch(Exception e){catalog=new JSONObject();}
        }
        JSONArray list=catalog.optJSONArray("themes");return list==null?new JSONArray():list;
    }
    private static File choiceFile(Context context){return new File(context.getFilesDir(),"theme.txt");}
    static String chosen(Context context){
        try{String id=new String(Files.readAllBytes(choiceFile(context).toPath()),StandardCharsets.UTF_8).trim();if(find(context,id)!=null)return id;}catch(Exception ignored){}
        return catalog!=null?catalog.optString("default","tonal"):"tonal";
    }
    static JSONObject find(Context context,String id){
        JSONArray list=themes(context);for(int i=0;i<list.length();i++){JSONObject t=list.optJSONObject(i);if(t!=null&&t.optString("id").equals(id))return t;}return null;
    }
    /** Loads the saved theme into Ui before any view is built. */
    static void apply(Context context){JSONObject theme=find(context,chosen(context));if(theme!=null)load(theme);}
    static void choose(Context context,String id){
        try{Files.write(choiceFile(context).toPath(),id.getBytes(StandardCharsets.UTF_8));}catch(Exception ignored){}
        apply(context);
    }
    private static int color(JSONObject theme,String key,int fallback){
        JSONObject android=theme.optJSONObject("android");String value=android!=null&&android.has(key)?android.optString(key):theme.optString(key,"");
        try{return value.isEmpty()?fallback:Color.parseColor(value);}catch(IllegalArgumentException e){return fallback;}
    }
    static void load(JSONObject t){
        Ui.BG=color(t,"bg",Ui.BG);Ui.PANEL=color(t,"panel",Ui.PANEL);Ui.CARD=color(t,"card",Ui.CARD);Ui.TEXT=color(t,"text",Ui.TEXT);
        Ui.MUTED=color(t,"textSecondary",Ui.MUTED);Ui.BLUE=color(t,"accent",Ui.BLUE);Ui.ACCENT_SOFT=color(t,"accentText",Ui.ACCENT_SOFT);
        Ui.TINT=color(t,"accentDeep",Ui.TINT);Ui.ON_ACCENT=color(t,"onAccent",Ui.ON_ACCENT);
        JSONObject seek=t.optJSONObject("seek");if(seek==null)return;
        Ui.SEEK_TRACK=color(seek,"track",Ui.SEEK_TRACK);
        JSONArray fill=seek.optJSONArray("fill");
        if(fill!=null&&fill.length()>0){try{Ui.SEEK_START=Color.parseColor(fill.optString(0));Ui.SEEK_END=Color.parseColor(fill.optString(fill.length()-1));}catch(IllegalArgumentException ignored){}}
        Ui.THUMB=color(seek,"thumb",Ui.THUMB);Ui.THUMB_BORDER=color(seek,"thumbBorder",Ui.THUMB_BORDER);
        // Phones get a slightly thicker bar than the desktop for touch.
        Ui.SEEK_HEIGHT=seek.optInt("height",4)+1;Ui.THUMB_SIZE=Math.max(14,seek.optInt("thumbSize",12)+2);
        Ui.THUMB_SHAPE=seek.optString("thumbShape","circle");Ui.THUMB_GLOW=seek.optBoolean("glow");
    }

    /** Theme picker; restyle rebuilds the caller's views without touching playback. */
    static void pick(Activity activity,Runnable restyle){
        Ui ui=new Ui(activity);Dialog dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);
        LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,16,16);Ui.insets(root);
        LinearLayout top=ui.row();TextView title=ui.title("Temas",22);ui.weight(top,title);top.addView(ui.icon("close","Fechar",dialog::dismiss),new LinearLayout.LayoutParams(ui.dp(48),ui.dp(48)));root.addView(top);
        root.addView(ui.text("Mudam cores, barra de progresso e marcador. Os mesmos temas existem no PC.",13,Ui.MUTED));ui.space(root,10);
        LinearLayout list=ui.column();ScrollView scroll=new ScrollView(activity);scroll.addView(list);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        String current=chosen(activity);JSONObject active=find(activity,current);JSONArray themes=themes(activity);
        for(int i=0;i<themes.length();i++){
            JSONObject t=themes.optJSONObject(i);if(t==null)continue;load(t);
            LinearLayout card=ui.column();ui.pad(card,14,12);ui.rounded(card,Ui.PANEL,14);
            boolean selected=t.optString("id").equals(current);
            if(selected){android.graphics.drawable.GradientDrawable outline=ui.background(Ui.PANEL,14);outline.setStroke(ui.dp(2),Ui.BLUE);card.setBackground(outline);}
            LinearLayout swatches=ui.row();for(int c:new int[]{Ui.BG,Ui.CARD,Ui.BLUE,Ui.ACCENT_SOFT}){View dot=new View(activity);swatch(ui,dot,c);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(ui.dp(18),ui.dp(18));p.setMarginEnd(ui.dp(6));swatches.addView(dot,p);}
            if(selected){TextView badge=ui.text("Em uso",12,Ui.ACCENT_SOFT);badge.setTypeface(null,android.graphics.Typeface.BOLD);swatches.addView(badge);}
            card.addView(swatches);ui.space(card,6);
            TextView name=ui.title(t.optString("name"),16);name.setTextColor(Ui.TEXT);card.addView(name);
            card.addView(ui.text(t.optString("description"),12,Ui.MUTED));
            SeekBar sample=new SeekBar(activity);sample.setMax(100);sample.setProgress(62);sample.setEnabled(false);ui.seek(sample);sample.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            card.addView(sample,new LinearLayout.LayoutParams(-1,ui.dp(36)));
            String id=t.optString("id");card.setContentDescription("Tema "+t.optString("name")+(selected?", em uso":""));
            ui.clickable(card,()->{choose(activity,id);dialog.dismiss();restyle.run();});
            LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);params.bottomMargin=ui.dp(10);list.addView(card,params);
        }
        if(active!=null)load(active);
        dialog.setContentView(root);dialog.show();
    }
    private static void swatch(Ui ui,View dot,int color){android.graphics.drawable.GradientDrawable d=ui.background(color,9);d.setStroke(1,0x40ffffff);dot.setBackground(d);}
}
