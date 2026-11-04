package app.tonal.android;

import java.util.LinkedHashMap;
import java.net.URI;
import java.util.Arrays;
import java.util.Locale;
import java.util.function.LongSupplier;

/** Session-only public discography cache and endpoint-family cooldowns. No audio work. */
final class SpotifyCatalogPolicy {
    private final LongSupplier clock;
    private final LinkedHashMap<String,Long> blockedUntil=new LinkedHashMap<>();
    private int epoch;
    private final LinkedHashMap<String,Page> pages=new LinkedHashMap<>();
    private static final class Page {
        final String json;final long expires;
        Page(String value,long until){json=value;expires=until;}
    }
    SpotifyCatalogPolicy(){this(()->System.nanoTime()/1_000_000L);}
    SpotifyCatalogPolicy(LongSupplier clock){this.clock=clock;}
    static String scope(String method,String url){
        String[] parts=URI.create(url).getPath().split("/",-1);
        if(parts.length>3&&Arrays.asList("artists","albums","playlists","tracks","shows","episodes","audiobooks","chapters","users").contains(parts[2]))parts[3]=":id";
        return method.toUpperCase(Locale.ROOT)+" "+String.join("/",parts);
    }
    private void expire(){blockedUntil.entrySet().removeIf(e->e.getValue()<=clock.getAsLong());}
    private String key(String method,String url){String key=scope(method,url);return !blockedUntil.containsKey(key)&&(blockedUntil.containsKey("overflow")||blockedUntil.size()>=64)?"overflow":key;}
    synchronized void limited(String method,String url,int seconds){
        expire();String key=key(method,url);
        blockedUntil.put(key,Math.max(blockedUntil.getOrDefault(key,0L),clock.getAsLong()+Math.max(1,seconds)*1000L));
    }
    synchronized int remaining(String method,String url){expire();return (int)Math.max(0,(blockedUntil.getOrDefault(key(method,url),0L)-clock.getAsLong()+999)/1000);}
    static boolean discography(String url){return url.startsWith("https://api.spotify.com/v1/artists/")&&url.split("\\?",2)[0].endsWith("/albums");}
    synchronized String cached(String url){
        Page page=pages.get(url);if(page==null)return null;
        if(page.expires<=clock.getAsLong()){pages.remove(url);return null;}return page.json;
    }
    synchronized void remember(String url,String json){
        remember(url,json,epoch);
    }
    synchronized int epoch(){return epoch;}
    synchronized void remember(String url,String json,int expectedEpoch){
        if(epoch!=expectedEpoch)return;
        if(!discography(url)||json.length()>65536)return;
        pages.entrySet().removeIf(e->e.getValue().expires<=clock.getAsLong());
        pages.remove(url);
        if(pages.size()>=32)pages.remove(pages.keySet().iterator().next());
        pages.put(url,new Page(json,clock.getAsLong()+600000));
    }
    synchronized void clear(){epoch++;pages.clear();} // keep server cooldown even on logout
}
