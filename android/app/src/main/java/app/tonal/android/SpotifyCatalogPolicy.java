package app.tonal.android;

import java.util.LinkedHashMap;
import java.util.function.LongSupplier;

/** Session-only public discography cache and shared Web API cooldown. No audio work. */
final class SpotifyCatalogPolicy {
    private final LongSupplier clock;
    private long blockedUntil;
    private int epoch;
    private final LinkedHashMap<String,Page> pages=new LinkedHashMap<>();
    private static final class Page {
        final String json;final long expires;
        Page(String value,long until){json=value;expires=until;}
    }
    SpotifyCatalogPolicy(){this(()->System.nanoTime()/1_000_000L);}
    SpotifyCatalogPolicy(LongSupplier clock){this.clock=clock;}
    synchronized void limited(int seconds){
        blockedUntil=Math.max(blockedUntil,clock.getAsLong()+Math.max(1,seconds)*1000L);
    }
    synchronized int remaining(){return (int)Math.max(0,(blockedUntil-clock.getAsLong()+999)/1000);}
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
