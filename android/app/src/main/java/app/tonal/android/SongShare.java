package app.tonal.android;

import android.app.*;
import android.content.*;
import android.graphics.*;
import android.net.Uri;
import android.view.*;
import android.widget.*;
import org.json.JSONObject;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;

final class SongShare {
    static String link(JSONObject t){String uri=t.optString("uri",t.optString("id"));return uri.matches("spotify:track:[A-Za-z0-9]{22}")?"https://open.spotify.com/track/"+uri.substring(14):"";}
    static void show(Activity a,JSONObject current){
        // Freeze track identity, including when playback advances behind the dialog.
        JSONObject track;try{track=new JSONObject(current.toString());}catch(Exception e){return;}
        String link=link(track);if(link.isEmpty()){Toast.makeText(a,"Esta fonte não forneceu um link Spotify de faixa.",Toast.LENGTH_LONG).show();return;}
        Ui ui=new Ui(a);Dialog d=new Dialog(a,android.R.style.Theme_Material_NoActionBar);LinearLayout root=ui.column();root.setBackgroundColor(Ui.BG);ui.pad(root,18,10);Ui.insets(root);
        LinearLayout header=ui.row();ui.weight(header,ui.title("Compartilhar música",23));header.addView(ui.icon("close","Fechar compartilhamento",d::dismiss));root.addView(header);
        Card preview=new Card(a,track);root.addView(preview,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout swatches=ui.row();String[] colors={"#9299B0","#009BD6","#7A538E","#BC634B","#101112"};
        EditText color=new EditText(a);color.setTextColor(Ui.TEXT);color.setSingleLine();color.setHint("#RRGGBB");color.setText(colors[0]);
        for(String c:colors){TextView b=ui.action("●",()->color.setText(c));b.setTextColor(Color.parseColor(c));swatches.addView(b,new LinearLayout.LayoutParams(0,ui.dp(48),1));}root.addView(swatches);root.addView(color);
        color.addTextChangedListener(new android.text.TextWatcher(){public void beforeTextChanged(CharSequence s,int st,int n,int after){}public void onTextChanged(CharSequence s,int st,int before,int n){if(s.toString().matches("#[a-fA-F0-9]{6}")){preview.color=Color.parseColor(s.toString());preview.invalidate();}}public void afterTextChanged(android.text.Editable e){}});
        Spinner style=new Spinner(a);style.setAdapter(new ArrayAdapter<>(a,android.R.layout.simple_spinner_dropdown_item,new String[]{"Cor sólida","Degradê","Papel / polaroid"}));style.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?> p,View v,int n,long id){preview.style=n;preview.invalidate();}public void onNothingSelected(AdapterView<?> p){}});root.addView(style);
        CheckBox cover=new CheckBox(a);cover.setText("Incluir capa do álbum");cover.setTextColor(Ui.TEXT);cover.setChecked(true);cover.setOnCheckedChangeListener((v,on)->{preview.cover=on;preview.invalidate();});root.addView(cover);
        LinearLayout actions=ui.row();ui.weight(actions,ui.action("Copiar link",()->{((android.content.ClipboardManager)a.getSystemService(Context.CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("Spotify",link));Toast.makeText(a,"Link copiado",Toast.LENGTH_SHORT).show();}));
        ui.weight(actions,ui.action("Enviar link",()->a.startActivity(Intent.createChooser(new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT,link),"Compartilhar música"))));root.addView(actions);
        TextView send=ui.action("Compartilhar cartão + link",()->{
            try{File dir=new File(a.getCacheDir(),"share-cards");dir.mkdirs();File[] files=dir.listFiles();if(files!=null){Arrays.sort(files,Comparator.comparingLong(File::lastModified).reversed());for(int i=0;i<files.length;i++)if(i>=7||System.currentTimeMillis()-files[i].lastModified()>86400000)files[i].delete();}
                File file=new File(dir,UUID.randomUUID().toString().replace("-","")+".png");Bitmap image=Bitmap.createBitmap(600,1066,Bitmap.Config.ARGB_8888);preview.paint(new Canvas(image));try(FileOutputStream out=new FileOutputStream(file)){image.compress(Bitmap.CompressFormat.PNG,100,out);}finally{image.recycle();}
                Uri uri=Uri.parse("content://"+a.getPackageName()+".sharecard/"+file.getName());Intent intent=new Intent(Intent.ACTION_SEND).setType("image/png").putExtra(Intent.EXTRA_STREAM,uri).putExtra(Intent.EXTRA_TEXT,link).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);intent.setClipData(ClipData.newUri(a.getContentResolver(),"Tonalify",uri));a.startActivity(Intent.createChooser(intent,"Compartilhar cartão e link"));
            }catch(Exception e){Toast.makeText(a,"Não foi possível preparar o cartão. Use Enviar link.",Toast.LENGTH_LONG).show();}});root.addView(send);
        ExecutorService worker=Executors.newSingleThreadExecutor();boolean[] closed={false};d.setOnDismissListener(v->{closed[0]=true;worker.shutdownNow();});d.setContentView(root);d.show();d.getWindow().setLayout(-1,-1);
        worker.execute(()->{Bitmap image=Artwork.fetch(a,track.optString("image"));a.runOnUiThread(()->{if(!closed[0]){preview.image=image;preview.invalidate();}});});
    }
    static final class Card extends View {
        final JSONObject track; Bitmap image;int color=0xff9299b0,style;boolean cover=true;final Paint p=new Paint(3);
        Card(Context c,JSONObject t){super(c);track=t;setContentDescription("Prévia do cartão de "+t.optString("title"));}
        @Override protected void onDraw(Canvas c){super.onDraw(c);float scale=Math.min(getWidth()/600f,getHeight()/1066f);c.save();c.translate((getWidth()-600*scale)/2,(getHeight()-1066*scale)/2);c.scale(scale,scale);paint(c);c.restore();}
        void paint(Canvas c){p.setShader(style==1?new LinearGradient(0,0,0,1066,color,0xff08090c,Shader.TileMode.CLAMP):null);p.setColor(style==2?0xfffaf5e9:color);c.drawRoundRect(0,0,600,1066,30,30,p);p.setShader(null);
            if(style==2){p.setColor(0xffd4e5e9);p.setStrokeWidth(2);for(int y=30;y<1066;y+=38)c.drawLine(0,y,600,y,p);}
            p.setColor(style==2?0xfffffbef:0xff161a22);c.drawRoundRect(45,125,555,935,22,22,p);
            if(cover&&image!=null)c.drawBitmap(image,null,new RectF(70,155,530,615),p);else{p.setColor(style==2?color:0xff263748);c.drawRoundRect(70,155,530,615,12,12,p);p.setColor(Color.WHITE);p.setTextSize(90);c.drawText("♪",270,410,p);}
            p.setColor(style==2?0xff111111:Color.WHITE);text(c,track.optString("title"),70,680,31,true,2);text(c,track.optString("artist"),70,775,24,false,2);text(c,"Tonalify · Ouvir no Spotify",70,875,20,false,1);
        }
        private void text(Canvas c,String text,int x,int y,int size,boolean bold,int lines){
            p.setTypeface(bold?Typeface.DEFAULT_BOLD:Typeface.DEFAULT);p.setTextSize(size);
            android.text.StaticLayout layout=android.text.StaticLayout.Builder.obtain(text,0,text.length(),new android.text.TextPaint(p),460)
                .setMaxLines(lines).setEllipsize(android.text.TextUtils.TruncateAt.END).setIncludePad(false)
                .setBreakStrategy(android.text.Layout.BREAK_STRATEGY_SIMPLE).build();
            c.save();c.translate(x,y-size);layout.draw(c);c.restore();
        }
    }
}
