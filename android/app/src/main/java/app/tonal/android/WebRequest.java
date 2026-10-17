package app.tonal.android;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import org.json.*;

/** HTTPS only, bounded responses, redirects and a hard wall-clock deadline. */
final class WebRequest {
    static final ScheduledExecutorService deadlines=Executors.newSingleThreadScheduledExecutor(r->{Thread t=new Thread(r,"indie-deadlines");t.setDaemon(true);return t;});
    static class Failure extends IOException {final int code;final String retryAfter;Failure(int c,String retry){super("HTTP "+c);code=c;retryAfter=retry;}}
    static URL url(String value)throws IOException{
        try{URI u=new URI(value);if(!"https".equalsIgnoreCase(u.getScheme())||u.getHost()==null||u.getUserInfo()!=null||u.getFragment()!=null)throw new IOException("Use um endereço HTTPS válido");return u.toURL();}catch(URISyntaxException e){throw new IOException("Endereço inválido");}
    }
    static String encode(String s){try{return URLEncoder.encode(s,"UTF-8");}catch(Exception impossible){throw new AssertionError(impossible);}}
    static String params(String base,String... pairs){StringBuilder b=new StringBuilder(base);for(int i=0;i<pairs.length;i+=2)b.append(b.indexOf("?")<0?'?':'&').append(encode(pairs[i])).append('=').append(encode(pairs[i+1]));return b.toString();}
    static HttpURLConnection open(String address,String range)throws IOException{
        for(int i=0;i<6;i++){
            HttpURLConnection c=(HttpURLConnection)url(address).openConnection();c.setInstanceFollowRedirects(false);c.setConnectTimeout(12000);c.setReadTimeout(12000);c.setRequestProperty("User-Agent","IndieTonal/0.4.0 (https://github.com/natheus/tonal-player)");c.setRequestProperty("Accept-Encoding","identity");if(range!=null)c.setRequestProperty("Range",range);
            ScheduledFuture<?> timeout=deadlines.schedule(c::disconnect,25,TimeUnit.SECONDS);
            int code;try{code=c.getResponseCode();}catch(IOException e){c.disconnect();throw e;}finally{timeout.cancel(false);}
            if(code==301||code==302||code==303||code==307||code==308){String next=c.getHeaderField("Location");c.disconnect();if(next==null)throw new IOException("Redirecionamento vazio");address=new URL(url(address),next).toString();continue;}
            if(code<200||code>=300){String retry=c.getHeaderField("Retry-After");c.disconnect();throw new Failure(code,retry==null?"":retry);}return c;
        }throw new IOException("Muitos redirecionamentos");
    }
    static JSONObject json(String address)throws Exception{
        return (JSONObject)value(address);
    }
    static Object value(String address)throws Exception{
        HttpURLConnection c=open(address,null);ScheduledFuture<?> timeout=deadlines.schedule(c::disconnect,30000,TimeUnit.MILLISECONDS);
        try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] bytes=new byte[16384];for(int n;(n=in.read(bytes))!=-1;){if(Thread.currentThread().isInterrupted())throw new InterruptedIOException();if(out.size()+n>6*1024*1024)throw new IOException("Resposta maior que 6 MiB");out.write(bytes,0,n);}return new JSONTokener(new String(out.toByteArray(),StandardCharsets.UTF_8)).nextValue();
        }finally{timeout.cancel(false);c.disconnect();}
    }
    private WebRequest(){}
}
