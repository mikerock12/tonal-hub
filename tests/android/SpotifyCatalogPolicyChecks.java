package app.tonal.android;

public final class SpotifyCatalogPolicyChecks {
    private static int checks;
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);checks++;}
    public static void main(String[] args){
        long[] now={1000};SpotifyCatalogPolicy policy=new SpotifyCatalogPolicy(()->now[0]);
        String url="https://api.spotify.com/v1/artists/band/albums?include_groups=album&limit=10";
        policy.remember(url,"albums");check("albums".equals(policy.cached(url)),"revisit cache");
        check(policy.cached(url.replace("=album&","=single&"))==null,"categories mixed");
        policy.remember("https://api.spotify.com/v1/me/tracks","private");check(policy.cached("https://api.spotify.com/v1/me/tracks")==null,"private data cached");
        policy.limited("GET",url,7200);check(policy.remaining("GET",url)==7200,"long Retry-After truncated");
        String album="https://api.spotify.com/v1/albums/record",playlist="https://api.spotify.com/v1/playlists/list";
        check(policy.remaining("GET",album)==0,"artist limit blocked album opening");
        check(policy.remaining("GET",playlist)==0,"artist limit blocked playlist opening");
        check(policy.remaining("GET","https://api.spotify.com/v1/search?q=band")==0,"artist limit blocked search");
        check(policy.remaining("GET",url.replace("/band/","/other/").replace("=album&","=single&"))==7200,"ID/category bypassed service wait");
        check(policy.remaining("POST",url)==0,"read wait leaked into another method");
        policy.limited("GET",album,120);check(policy.remaining("GET",playlist)==0,"album limit blocked playlist");
        check(policy.remaining("GET",album+"?market=BR")==120,"query bypassed album wait");
        now[0]+=1500;check(policy.remaining("GET",url)==7199,"remaining wait rounded down");
        policy.limited("GET",url,1);check(policy.remaining("GET",url)==7199,"short response shortened active cooldown");
        check("albums".equals(policy.cached(url)),"cache unavailable while blocked");
        int epoch=policy.epoch();policy.clear();check(policy.cached(url)==null&&policy.remaining("GET",url)==7199,"logout cache/cooldown");
        policy.remember(url,"late previous account",epoch);check(policy.cached(url)==null,"late response repopulated cleared cache");
        now[0]+=7200000;check(policy.remaining("GET",url)==0,"cooldown failed to expire");
        policy.remember(url,"albums");now[0]+=600000;check(policy.cached(url)==null,"TTL expiration");
        for(int i=0;i<33;i++)policy.remember(url+"&offset="+i,"page"+i);
        check(policy.cached(url+"&offset=0")==null&&"page32".equals(policy.cached(url+"&offset=32")),"bounded pages");
        policy.remember(url,"x".repeat(65537));check(policy.cached(url)==null,"oversize page cached");
        System.out.println("SPOTIFY_CATALOG_POLICY_PASS: "+checks+" checks");
    }
}
