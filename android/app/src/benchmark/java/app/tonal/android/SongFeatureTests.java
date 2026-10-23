package app.tonal.android;

import android.app.Instrumentation;
import android.content.*;
import android.graphics.*;
import android.net.Uri;
import org.json.*;
import java.io.*;

/** Account-free regression checks of the production parser, share renderer and
 * restricted provider. Never publishes a playlist or sends a message. */
final class SongFeatureTests {
    static String run(Instrumentation instrumentation)throws Exception{
        JSONArray checks=new JSONArray();Context context=instrumentation.getTargetContext();
        JSONObject track=new JSONObject().put("uri","spotify:track:0123456789ABCDEFGHIJKL").put("title","Música de teste · dados sintéticos").put("artist","Artista de teste");
        check(SongShare.link(track).equals("https://open.spotify.com/track/0123456789ABCDEFGHIJKL"),"share_canonical_track_link",checks);
        check(SongShare.link(new JSONObject().put("uri","spotify:artist:0123456789ABCDEFGHIJKL")).isEmpty(),"share_rejects_nontrack_and_missing_id",checks);
        JSONObject response=new JSONObject("{\"setlist\":[{\"id\":\"test\",\"artist\":{\"name\":\"Artista\"},\"sets\":{\"set\":[{\"song\":[{\"name\":\"Intro\",\"tape\":true},{\"name\":\"Faixa\"},{\"name\":\"Cover\",\"cover\":{\"name\":\"Autor original\"}},{\"name\":\"\"}]},{\"encore\":1,\"song\":[{\"name\":\"Faixa\"}]}]}}]}");
        JSONArray songs=SetlistWizard.flatten(response).getJSONObject(0).getJSONArray("songs");
        check(songs.length()==3&&songs.getString(0).equals(songs.getString(2)),"setlist_preserves_encore_order_and_reprises",checks);
        check(songs.getString(1).equals("Artista — Cover"),"setlist_cover_keeps_show_performer",checks);
        check(SetlistWizard.flatten(new JSONObject()).length()==0,"setlist_empty_result",checks);
        JSONArray many=new JSONArray();for(int n=0;n<501;n++)many.put(new JSONObject().put("name","Faixa "+n));JSONObject large=new JSONObject().put("artist",new JSONObject().put("name","Artista")).put("sets",new JSONObject().put("set",new JSONArray().put(new JSONObject().put("song",many)).put(new JSONObject().put("song",new JSONArray().put(new JSONObject().put("name","Encore"))))));
        check(SetlistWizard.flatten(new JSONObject().put("setlist",new JSONArray().put(large))).getJSONObject(0).getJSONArray("songs").length()==500,"setlist_cap_applies_across_all_sets",checks);
        JSONObject facts=ArtistNotes.facts(new JSONObject().put("type","Group").put("life-span",new JSONObject().put("begin","1994")));
        check(facts.getJSONArray("facts").getString(0).equals("Formação: 1994"),"artist_sourced_formation",checks);
        check(ArtistNotes.facts(new JSONObject()).getJSONArray("facts").length()==0,"artist_missing_values_not_invented",checks);
        JSONObject homonyms=new JSONObject().put("artists",new JSONArray().put(new JSONObject().put("name","Band").put("score",100)).put(new JSONObject().put("name","Band").put("score",100)));
        check(MusicMetadata.uniqueArtist(homonyms,"Band")==null,"ambiguous_artist_not_attributed",checks);
        JSONObject releases=new JSONObject("{\"release-groups\":[{\"title\":\"Test Album\",\"artist-credit\":[{\"artist\":{\"id\":\"one\",\"name\":\"Band\"}}]}]}");
        check(ArtistNotes.artistFromRelease(releases,"Band","Test Album").equals("one"),"artist_album_disambiguates_homonyms",checks);
        releases.getJSONArray("release-groups").put(new JSONObject("{\"title\":\"Test Album\",\"artist-credit\":[{\"artist\":{\"id\":\"two\",\"name\":\"Band\"}}]}"));
        check(ArtistNotes.artistFromRelease(releases,"Band","Test Album").isEmpty(),"ambiguous_album_still_abstains",checks);
        check(!context.getPackageManager().resolveContentProvider(context.getPackageName()+".sharecard",0).exported,"provider_not_exported",checks);
        for(int style=0;style<3;style++){final int selection=style;instrumentation.runOnMainSync(()->{SongShare.Card card=new SongShare.Card(context,track);card.style=selection;Bitmap bitmap=Bitmap.createBitmap(600,1066,Bitmap.Config.ARGB_8888);card.paint(new Canvas(bitmap));if(bitmap.getPixel(5,500)==Color.TRANSPARENT)throw new AssertionError("blank share render");bitmap.recycle();});checks.put("share_renderer_style_"+style);}
        File dir=new File(context.getCacheDir(),"share-cards");dir.mkdirs();File card=new File(dir,"0123456789abcdef0123456789abcdef.png");try(FileOutputStream out=new FileOutputStream(card)){out.write(new byte[]{1,2,3});}
        Uri uri=Uri.parse("content://"+context.getPackageName()+".sharecard/"+card.getName());
        try(InputStream in=context.getContentResolver().openInputStream(uri)){check(in.read()==1,"provider_reads_generated_card",checks);}
        boolean blocked=false;try{context.getContentResolver().openFileDescriptor(uri,"w");}catch(FileNotFoundException e){blocked=true;}check(blocked,"provider_refuses_write",checks);
        blocked=false;try{context.getContentResolver().openInputStream(Uri.parse("content://"+context.getPackageName()+".sharecard/../files/secrets.xml"));}catch(FileNotFoundException e){blocked=true;}check(blocked,"provider_refuses_path_traversal",checks);card.delete();
        return new JSONObject().put("passed",true).put("checks",checks).toString();
    }
    private static void check(boolean yes,String label,JSONArray checks){if(!yes)throw new AssertionError(label);checks.put(label);}
}
