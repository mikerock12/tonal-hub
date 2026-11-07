package app.tonal.android;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Which styles Discovery searches. Spotify may send no genres at all, so one unknown
 * anchor must not end the search while its neighbours in the selection are known.
 */
final class DiscoveryStyles {
    static final int LIMIT=3;
    private DiscoveryStyles(){}
    private static List<String> tags(String value){
        ArrayList<String> out=new ArrayList<>();if(value==null)return out;
        for(String tag:value.split("[,;|]")){String clean=tag.trim().toLowerCase(java.util.Locale.ROOT);if(!clean.isEmpty()&&!out.contains(clean))out.add(clean);}
        return out;
    }
    /** Mostly the anchor's own styles, then one from each neighbour for variety. */
    static String merge(String anchor,List<String> rows){
        LinkedHashSet<String> picked=new LinkedHashSet<>();List<String> own=tags(anchor);
        for(int i=0;i<Math.min(2,own.size());i++)picked.add(own.get(i));
        for(String row:rows){if(picked.size()>=LIMIT)break;for(String tag:tags(row))if(picked.add(tag))break;}
        for(String tag:own)if(picked.size()<LIMIT)picked.add(tag);
        for(String row:rows)for(String tag:tags(row))if(picked.size()<LIMIT)picked.add(tag);
        return String.join(",",picked);
    }
}
