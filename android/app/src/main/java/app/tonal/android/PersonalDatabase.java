package app.tonal.android;
import android.content.*;
import android.database.Cursor;
import android.database.sqlite.*;
import org.json.*;
import java.io.File;
import java.util.*;

/** Schema 1, normalized rows and transactional incremental updates, written off the UI thread. */
final class PersonalDatabase extends SQLiteOpenHelper {
    private final File path;private JSONObject previous=new JSONObject();
    PersonalDatabase(Context c,String name){super(c,name,null,1);path=c.getDatabasePath(name);setWriteAheadLoggingEnabled(true);}
    @Override public void onCreate(SQLiteDatabase db){
        db.execSQL("CREATE TABLE settings(id TEXT PRIMARY KEY,json TEXT NOT NULL)");
        db.execSQL("CREATE TABLE tracks(id TEXT PRIMARY KEY,provider TEXT,title TEXT,artist TEXT,genre TEXT,json TEXT NOT NULL)");
        db.execSQL("CREATE TABLE listens(id TEXT PRIMARY KEY,track_id TEXT,started_at INTEGER,listened_ms INTEGER,qualified INTEGER,json TEXT NOT NULL)");
        db.execSQL("CREATE TABLE favorites(id TEXT PRIMARY KEY,position INTEGER,json TEXT NOT NULL)");
        db.execSQL("CREATE TABLE playlists(id TEXT PRIMARY KEY,name TEXT,position INTEGER,json TEXT NOT NULL)");
        db.execSQL("CREATE TABLE playlist_items(playlist_id TEXT,position INTEGER,track_id TEXT,PRIMARY KEY(playlist_id,position))");
        db.execSQL("CREATE INDEX listens_date ON listens(started_at)");db.execSQL("CREATE INDEX tracks_artist ON tracks(artist)");
    }
    @Override public void onUpgrade(SQLiteDatabase db,int oldVersion,int newVersion){throw new SQLiteException("Versão de biblioteca não suportada");}
    JSONObject read()throws Exception{if(!path.exists())return new JSONObject();SQLiteDatabase db=getReadableDatabase();JSONObject result=new JSONObject();try(Cursor c=db.rawQuery("SELECT json FROM settings WHERE id='preferences'",null)){if(c.moveToFirst())result=new JSONObject(c.getString(0));}
        JSONObject tracks=new JSONObject();try(Cursor c=db.rawQuery("SELECT id,json FROM tracks",null)){while(c.moveToNext())tracks.put(c.getString(0),new JSONObject(c.getString(1)));}result.put("tracks",tracks);
        for(String table:new String[]{"listens","favorites","playlists"}){JSONArray list=new JSONArray();try(Cursor c=db.rawQuery("SELECT json FROM "+table+" ORDER BY "+(table.equals("listens")?"started_at,id":"position"),null)){while(c.moveToNext()){JSONObject row=new JSONObject(c.getString(0));row.remove("position");list.put(table.equals("favorites")?row.optString("key"):row);}}result.put(table.equals("listens")?"events":table,list);}
        previous=new JSONObject(result.toString());return result;
    }
    private static Map<String,JSONObject> rows(JSONObject data,String table)throws Exception{
        LinkedHashMap<String,JSONObject> result=new LinkedHashMap<>();
        if(table.equals("tracks")){JSONObject all=MusicStore.object(data,"tracks");for(Iterator<String> it=all.keys();it.hasNext();){String k=it.next();result.put(k,all.getJSONObject(k));}return result;}
        if(table.equals("settings")){JSONObject settings=new JSONObject(data.toString());for(String k:new String[]{"tracks","events","favorites","playlists"})settings.remove(k);result.put("preferences",settings);return result;}
        JSONArray array=MusicStore.array(data,table.equals("listens")?"events":table);for(int i=0;i<array.length();i++){JSONObject row=table.equals("favorites")?new JSONObject().put("key",array.getString(i)).put("position",i):new JSONObject(array.getJSONObject(i).toString());if(table.equals("playlists"))row.put("position",i);String id=table.equals("favorites")?array.getString(i):table.equals("playlists")?row.optString("id"):row.optString("session");if(id.isEmpty())id=i+"|"+row.optString("key");result.put(id,row);}return result;
    }
    void write(String snapshot)throws Exception{JSONObject data=new JSONObject(snapshot);SQLiteDatabase db=getWritableDatabase();db.beginTransaction();try{
        for(String table:new String[]{"settings","tracks","listens","favorites","playlists"}){Map<String,JSONObject> next=rows(data,table),old=rows(previous,table);
            for(String id:old.keySet())if(!next.containsKey(id))db.delete(table,"id=?",new String[]{id});
            for(Map.Entry<String,JSONObject> entry:next.entrySet()){JSONObject row=entry.getValue();if(old.containsKey(entry.getKey())&&old.get(entry.getKey()).toString().equals(row.toString()))continue;ContentValues values=new ContentValues();values.put("id",entry.getKey());values.put("json",row.toString());
                if(table.equals("tracks"))for(String k:new String[]{"provider","title","artist","genre"})values.put(k,row.optString(k));
                if(table.equals("listens")){values.put("track_id",row.optString("key"));values.put("started_at",row.optLong("at"));values.put("listened_ms",row.optLong("ms"));values.put("qualified",row.optBoolean("play")?1:0);}
                if(table.equals("favorites")||table.equals("playlists"))values.put("position",row.optInt("position"));if(table.equals("playlists"))values.put("name",row.optString("name"));if(db.insertWithOnConflict(table,null,values,SQLiteDatabase.CONFLICT_REPLACE)<0)throw new SQLiteException("Falha ao gravar biblioteca");
            }
        }
        if(!MusicStore.array(previous,"playlists").toString().equals(MusicStore.array(data,"playlists").toString())){db.delete("playlist_items",null,null);for(Object value:MusicStore.values(MusicStore.array(data,"playlists"))){JSONObject p=(JSONObject)value;JSONArray entries=MusicStore.array(p,"tracks");for(int n=0;n<entries.length();n++){ContentValues row=new ContentValues();row.put("playlist_id",p.optString("id"));row.put("position",n);row.put("track_id",entries.optString(n));db.insertOrThrow("playlist_items",null,row);}}}
        db.setTransactionSuccessful();
    }finally{db.endTransaction();}previous=data;}
}
