package app.tonal.android;

import android.app.Activity;
import android.graphics.*;
import android.util.LruCache;
import android.widget.ImageView;
import java.io.*;
import java.net.*;
import java.util.concurrent.*;
import org.json.*;

/** Bounded, sampled artwork loading. Catalog bearer tokens never reach image servers. */
final class Artwork implements AutoCloseable {
    private final Activity activity;
    private volatile boolean closed;
    private final LruCache<String,Bitmap> cache=new LruCache<>(6*1024*1024){protected int sizeOf(String key,Bitmap b){return b.getAllocationByteCount();}};
    private final ThreadPoolExecutor worker=new ThreadPoolExecutor(2,2,30,TimeUnit.SECONDS,new ArrayBlockingQueue<>(96),new ThreadPoolExecutor.DiscardOldestPolicy());
    Artwork(Activity activity){this.activity=activity;}
    void loadLocal(ImageView view,android.net.Uri uri){view.setTag(uri.toString());if(closed)return;worker.execute(()->{try{
        BitmapFactory.Options options=new BitmapFactory.Options();options.inJustDecodeBounds=true;
        try(InputStream in=activity.getContentResolver().openInputStream(uri)){BitmapFactory.decodeStream(in,null,options);}
        options.inSampleSize=1;while(options.outWidth/options.inSampleSize>1280||options.outHeight/options.inSampleSize>1280)options.inSampleSize*=2;options.inJustDecodeBounds=false;
        Bitmap image;try(InputStream in=activity.getContentResolver().openInputStream(uri)){image=BitmapFactory.decodeStream(in,null,options);}
        if(image!=null&&!closed)activity.runOnUiThread(()->{if(!closed&&uri.toString().equals(view.getTag()))view.setImageBitmap(image);});
    }catch(Exception ignored){}});}
    static String url(JSONObject item){if(item==null)return "";JSONArray images=item.optJSONArray("images");if(images==null){JSONObject album=item.optJSONObject("album");if(album!=null)return url(album);}if(images==null)return "";String result="";for(int i=0;i<images.length();i++){JSONObject image=images.optJSONObject(i);if(image==null)continue;result=image.optString("url");if(image.optInt("width",300)<=320)break;}return result;}
    void load(ImageView view,String url){view.setTag(url);view.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));if(url==null||url.isEmpty())return;Bitmap bitmap=cache.get(url);if(bitmap!=null){view.setImageBitmap(bitmap);return;}if(closed)return;
        worker.execute(()->{try{URI uri=URI.create(url);if(!"https".equals(uri.getScheme())||uri.getHost()==null||uri.getUserInfo()!=null)return;HttpURLConnection c=(HttpURLConnection)uri.toURL().openConnection();try{c.setConnectTimeout(10000);c.setReadTimeout(10000);c.setInstanceFollowRedirects(false);if(c.getResponseCode()!=200)return;ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream input=c.getInputStream()){byte[] block=new byte[8192];int n;while((n=input.read(block))!=-1){if(bytes.size()+n>3*1024*1024)return;bytes.write(block,0,n);}}byte[] data=bytes.toByteArray();BitmapFactory.Options options=new BitmapFactory.Options();options.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(data,0,data.length,options);options.inSampleSize=1;while(options.outWidth/options.inSampleSize>384||options.outHeight/options.inSampleSize>384)options.inSampleSize*=2;options.inJustDecodeBounds=false;Bitmap image=BitmapFactory.decodeByteArray(data,0,data.length,options);if(image==null||closed)return;cache.put(url,image);activity.runOnUiThread(()->{if(!closed&&url.equals(view.getTag()))view.setImageBitmap(image);});}finally{c.disconnect();}}catch(Exception ignored){/* Keep a stable placeholder when artwork is unavailable. */}});
    }
    public void close(){closed=true;worker.shutdownNow();cache.evictAll();}
}
