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
    private static final LruCache<String,Bitmap> cache=new LruCache<>(8*1024*1024){protected int sizeOf(String key,Bitmap b){return b.getAllocationByteCount();}};
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
    private File disk(String url)throws Exception{byte[] digest=java.security.MessageDigest.getInstance("SHA-256").digest(url.getBytes(java.nio.charset.StandardCharsets.UTF_8));StringBuilder name=new StringBuilder();for(byte b:digest)name.append(String.format(java.util.Locale.ROOT,"%02x",b&255));File dir=new File(activity.getCacheDir(),"artwork-v2");dir.mkdirs();return new File(dir,name.toString());}
    private void trim(File keep){File[] files=keep.getParentFile().listFiles();if(files==null)return;java.util.Arrays.sort(files,java.util.Comparator.comparingLong(File::lastModified).reversed());long bytes=0;for(File f:files){bytes+=f.length();if(bytes>32L*1024*1024&&!f.equals(keep))f.delete();}}
    private Bitmap decode(byte[] data){BitmapFactory.Options options=new BitmapFactory.Options();options.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(data,0,data.length,options);if(options.outWidth<=0||options.outHeight<=0)return null;options.inSampleSize=1;while(options.outWidth/options.inSampleSize>768||options.outHeight/options.inSampleSize>768)options.inSampleSize*=2;options.inJustDecodeBounds=false;return BitmapFactory.decodeByteArray(data,0,data.length,options);}
    static String large(JSONObject item){if(item==null)return "";JSONArray images=item.optJSONArray("images");if(images==null)return item.optJSONObject("album")!=null?large(item.optJSONObject("album")):"";String result="";int width=-1;for(Object v:MusicStore.values(images)){JSONObject image=(JSONObject)v;int size=image.optInt("width",0);if(!image.optString("url").isEmpty()&&size>width){width=size;result=image.optString("url");}}return result;}
    void load(ImageView view,String url){view.setForeground(Ui.HALLOWEEN||Ui.CHRISTMAS?new SeasonalDrawables.Cover(activity):null);String address=url==null?"":url;if(address.equals(view.getTag())&&view.getDrawable()!=null)return;view.setTag(address);view.setImageDrawable(new Ui.Symbol("image",Ui.MUTED));if(address.isEmpty()||closed)return;Bitmap bitmap=cache.get(address);if(bitmap!=null){view.setImageBitmap(bitmap);return;}
        if(address.startsWith("data:image/jpeg;base64,")){if(address.length()>262200)return;worker.execute(()->{try{byte[] data=android.util.Base64.decode(address.substring(address.indexOf(',')+1),android.util.Base64.DEFAULT);Bitmap image=decode(data);if(image!=null&&!closed)activity.runOnUiThread(()->{if(!closed&&address.equals(view.getTag()))view.setImageBitmap(image);});}catch(Exception ignored){}});return;}
        worker.execute(()->{try{File file=disk(address);Bitmap image=null;if(file.isFile()&&file.length()<=3*1024*1024){image=decode(java.nio.file.Files.readAllBytes(file.toPath()));file.setLastModified(System.currentTimeMillis());}
            for(int attempt=0;image==null&&attempt<3&&!closed;attempt++){try{HttpURLConnection c=WebRequest.open(address,null);ScheduledFuture<?> deadline=WebRequest.deadlines.schedule(c::disconnect,20,TimeUnit.SECONDS);try{ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=c.getInputStream()){byte[] block=new byte[8192];int n;while((n=in.read(block))!=-1){if(bytes.size()+n>3*1024*1024)throw new IOException();bytes.write(block,0,n);}}byte[] data=bytes.toByteArray();image=decode(data);if(image!=null){File temp=new File(file.getPath()+"."+Thread.currentThread().getId()+".tmp");try(FileOutputStream out=new FileOutputStream(temp)){out.write(data);}java.nio.file.Files.move(temp.toPath(),file.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);trim(file);}}finally{deadline.cancel(false);c.disconnect();}}catch(Exception e){if(attempt<2)Thread.sleep(500L*(attempt+1));}}
            if(image!=null&&!closed){cache.put(address,image);Bitmap ready=image;activity.runOnUiThread(()->{if(!closed&&address.equals(view.getTag()))view.setImageBitmap(ready);});}
        }catch(Exception ignored){}});
    }
    public void close(){closed=true;worker.shutdownNow();}

    /**
     * Cover for the media notification and lock screen, off the main thread:
     * memory cache, then the same disk cache the screens fill, then a bounded
     * download (3 MiB, 20 s). Returns null when unavailable; never throws.
     */
    static Bitmap fetch(android.content.Context context,String address){
        if(address==null||address.isEmpty()||!address.startsWith("https://"))return null;
        Bitmap hit=cache.get(address);if(hit!=null)return hit;
        try{
            byte[] digest=java.security.MessageDigest.getInstance("SHA-256").digest(address.getBytes(java.nio.charset.StandardCharsets.UTF_8));StringBuilder name=new StringBuilder();for(byte b:digest)name.append(String.format(java.util.Locale.ROOT,"%02x",b&255));
            File dir=new File(context.getCacheDir(),"artwork-v2");dir.mkdirs();File file=new File(dir,name.toString());byte[] data=null;
            if(file.isFile()&&file.length()<=3*1024*1024)data=java.nio.file.Files.readAllBytes(file.toPath());
            if(data==null){HttpURLConnection c=WebRequest.open(address,null);ScheduledFuture<?> deadline=WebRequest.deadlines.schedule(c::disconnect,20,TimeUnit.SECONDS);
                try{ByteArrayOutputStream bytes=new ByteArrayOutputStream();try(InputStream in=c.getInputStream()){byte[] block=new byte[8192];int n;while((n=in.read(block))!=-1){if(bytes.size()+n>3*1024*1024)return null;bytes.write(block,0,n);}}data=bytes.toByteArray();
                    File temp=new File(file.getPath()+".notification.tmp");try(FileOutputStream out=new FileOutputStream(temp)){out.write(data);}java.nio.file.Files.move(temp.toPath(),file.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);
                }finally{deadline.cancel(false);c.disconnect();}}
            BitmapFactory.Options options=new BitmapFactory.Options();options.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(data,0,data.length,options);if(options.outWidth<=0||options.outHeight<=0)return null;
            options.inSampleSize=1;while(options.outWidth/options.inSampleSize>512||options.outHeight/options.inSampleSize>512)options.inSampleSize*=2;options.inJustDecodeBounds=false;
            Bitmap image=BitmapFactory.decodeByteArray(data,0,data.length,options);if(image!=null)cache.put(address,image);return image;
        }catch(Exception e){return null;}
    }
}
