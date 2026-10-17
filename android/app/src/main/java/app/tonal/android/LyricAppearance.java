package app.tonal.android;

import android.app.*;
import android.graphics.*;
import android.widget.*;
import org.json.*;

final class LyricAppearance {
    static final String[] MODES={"Aurora","Ondas","Espectro","Capa","Foto","Estrelas","Chuva","Orbitas","Pulso","Grade","Sem animação"};
    static Typeface font(String name){return Typeface.create(name.equals("Serif")?"serif":name.equals("Mono")?"monospace":name.equals("Rounded")?"sans-serif-rounded":"sans-serif",Typeface.NORMAL);}
    static void apply(TextView view,JSONObject style,boolean translation){String prefix=translation?"translation":"lyric";view.setTypeface(font(style.optString(prefix+"Font")),translation?Typeface.NORMAL:Typeface.BOLD);view.setTextColor(Color.parseColor(style.optString(prefix+"Color")));view.setTextSize(style.optInt(prefix+"Size"));}
    static void show(Activity a,MusicStore store,Runnable changed){Ui ui=new Ui(a);LinearLayout body=ui.column();ui.pad(body,18,12);JSONObject style=store.lyricStyle();
        for(String prefix:new String[]{"lyric","translation"}){body.addView(ui.title(prefix.equals("lyric")?"Letra original":"Tradução",20));String[] fonts={"Sans","Serif","Mono","Rounded"};Spinner font=new Spinner(a);font.setAdapter(new ArrayAdapter<>(a,android.R.layout.simple_spinner_dropdown_item,fonts));for(int i=0;i<fonts.length;i++)if(fonts[i].equals(style.optString(prefix+"Font")))font.setSelection(i);font.setContentDescription("Fonte da "+(prefix.equals("lyric")?"letra":"tradução"));font.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener(){public void onNothingSelected(android.widget.AdapterView<?> p){}public void onItemSelected(android.widget.AdapterView<?> p,android.view.View v,int n,long id){store.setLyricStyle(prefix+"Font",fonts[n]);changed.run();}});body.addView(font);
            LinearLayout colors=ui.row();for(String color:new String[]{"#F5F5F5","#CEE9F0","#7DDBFF","#FFE39C","#FFBBD9","#AEE8B7"}){TextView swatch=ui.text("●",28,Color.parseColor(color));swatch.setGravity(android.view.Gravity.CENTER);swatch.setContentDescription("Cor "+color+" da "+prefix);ui.clickable(swatch,()->{store.setLyricStyle(prefix+"Color",color);changed.run();});ui.weight(colors,swatch);}body.addView(colors);
            EditText custom=new EditText(a);custom.setSingleLine();custom.setHint("Cor hexadecimal #RRGGBB");custom.setText(style.optString(prefix+"Color"));custom.setTextColor(Ui.TEXT);body.addView(custom);body.addView(ui.action("Aplicar cor",()->{String c=custom.getText().toString().trim();if(c.matches("#[0-9a-fA-F]{6}")){store.setLyricStyle(prefix+"Color",c);changed.run();}else custom.setError("Use #RRGGBB");}));
            SeekBar size=new SeekBar(a);size.setMax(28);size.setProgress(style.optInt(prefix+"Size")-16);size.setContentDescription("Tamanho da "+prefix);size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar b,int value,boolean user){if(user){store.setLyricStyle(prefix+"Size",16+value);changed.run();}}public void onStartTrackingTouch(SeekBar b){}public void onStopTrackingTouch(SeekBar b){}});body.addView(size);ui.space(body,12);
        }ScrollView scroll=new ScrollView(a);scroll.addView(body);new AlertDialog.Builder(a).setTitle("Fontes e cores das letras").setView(scroll).setPositiveButton("Concluir",null).show();
    }
}
