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
        JSONObject tracks=new JSONObject();try(Cursor c=db.rawQuery("SELECT id,json FROM tracks WHERE id IN (SELECT id FROM tracks ORDER BY rowid DESC LIMIT 3000) OR id IN (SELECT id FROM favorites) OR id IN (SELECT track_id FROM playlist_items)",null)){while(c.moveToNext())tracks.put(c.getString(0),new JSONObject(c.getString(1)));}result.put("tracks",tracks);
        for(String table:new String[]{"listens","favorites","playlists"}){JSONArray list=new JSONArray();try(Cursor c=db.rawQuery(table.equals("listens")?"SELECT json FROM (SELECT id,started_at,json FROM listens ORDER BY started_at DESC,id DESC LIMIT 10000) ORDER BY started_at,id":"SELECT json FROM "+table+" ORDER BY position",null)){while(c.moveToNext()){JSONObject row=new JSONObject(c.getString(0));row.remove("position");list.put(table.equals("favorites")?row.optString("key"):row);}}result.put(table.equals("listens")?"events":table,list);}
        previous=new JSONObject(result.toString());return result;
    }
    private static Map<String,JSONObject> rows(JSONObject data,String table)throws Exception{
        LinkedHashMap<String,JSONObject> result=new LinkedHashMap<>();
        if(table.equals("tracks")){JSONObject all=MusicStore.object(data,"tracks");for(Iterator<String> it=all.keys();it.hasNext();){String k=it.next();result.put(k,all.getJSONObject(k));}return result;}
        if(table.equals("settings")){JSONObject settings=new JSONObject(data.toString());for(String k:new String[]{"tracks","events","favorites","playlists"})settings.remove(k);result.put("preferences",settings);return result;}
        JSONArray array=MusicStore.array(data,table.equals("listens")?"events":table);for(int i=0;i<array.length();i++){JSONObject row=table.equals("favorites")?new JSONObject().put("key",array.getString(i)).put("position",i):new JSONObject(array.getJSONObject(i).toString());if(table.equals("playlists"))row.put("position",i);String id=table.equals("favorites")?array.getString(i):table.equals("playlists")?row.optString("id"):row.optString("session");if(id.isEmpty())id=i+"|"+row.optString("key");result.put(id,row);}return result;
    }
    void write(String snapshot)throws Exception{JSONObject data=new JSONObject(snapshot);SQLiteDatabase db=getWritableDatabase();db.beginTransaction();try{
        JSONObject oldData=previous;
        if(!data.optString("libraryEpoch").equals(previous.optString("libraryEpoch"))){for(String t:new String[]{"settings","tracks","listens","favorites","playlists","playlist_items"})db.delete(t,null,null);oldData=new JSONObject();}
        else if(!data.optString("historyEpoch").equals(previous.optString("historyEpoch"))){db.delete("listens",null,null);oldData=new JSONObject(previous.toString());oldData.put("events",new JSONArray());}
        for(String table:new String[]{"settings","tracks","listens","favorites","playlists"}){Map<String,JSONObject> next=rows(data,table),old=rows(oldData,table);
            for(String id:old.keySet())if(!table.equals("listens")&&!next.containsKey(id))db.delete(table,"id=?"+(table.equals("tracks")?" AND NOT EXISTS(SELECT 1 FROM listens WHERE track_id=tracks.id)":""),new String[]{id});
            for(Map.Entry<String,JSONObject> entry:next.entrySet()){JSONObject row=entry.getValue();if(old.containsKey(entry.getKey())&&old.get(entry.getKey()).toString().equals(row.toString()))continue;ContentValues values=new ContentValues();values.put("id",entry.getKey());values.put("json",row.toString());
                if(table.equals("tracks"))for(String k:new String[]{"provider","title","artist","genre"})values.put(k,row.optString(k));
                if(table.equals("listens")){values.put("track_id",row.optString("key"));values.put("started_at",row.optLong("at"));values.put("listened_ms",row.optLong("ms"));values.put("qualified",row.optBoolean("play")?1:0);}
                if(table.equals("favorites")||table.equals("playlists"))values.put("position",row.optInt("position"));if(table.equals("playlists"))values.put("name",row.optString("name"));if(db.insertWithOnConflict(table,null,values,SQLiteDatabase.CONFLICT_REPLACE)<0)throw new SQLiteException("Falha ao gravar biblioteca");
            }
        }
        if(!MusicStore.array(oldData,"playlists").toString().equals(MusicStore.array(data,"playlists").toString())){db.delete("playlist_items",null,null);for(Object value:MusicStore.values(MusicStore.array(data,"playlists"))){JSONObject p=(JSONObject)value;JSONArray entries=MusicStore.array(p,"tracks");for(int n=0;n<entries.length();n++){ContentValues row=new ContentValues();row.put("playlist_id",p.optString("id"));row.put("position",n);row.put("track_id",entries.optString(n));db.insertOrThrow("playlist_items",null,row);}}}
        db.setTransactionSuccessful();
    }finally{db.endTransaction();}previous=data;}

    long bytes(){return path.length()+new File(path+"-wal").length()+new File(path+"-shm").length();}
    JSONObject historySnapshot(JSONObject current,long cutoff)throws Exception{
        JSONObject result=new JSONObject(current.toString()),tracks=MusicStore.object(result,"tracks");JSONArray events=MusicStore.array(result,"events");SQLiteDatabase db=getReadableDatabase();
        db.beginTransactionNonExclusive();try{
        db.execSQL("DROP TABLE IF EXISTS temp.active_sessions");db.execSQL("CREATE TEMP TABLE active_sessions(id TEXT PRIMARY KEY)");
        try(SQLiteStatement insert=db.compileStatement("INSERT OR IGNORE INTO active_sessions VALUES(?)")){for(int i=0;i<events.length();i++){JSONObject e=events.optJSONObject(i);String id=e.optString("session");if(id.isEmpty())id=i+"|"+e.optString("key");insert.bindString(1,id);insert.executeInsert();}}
        try(Cursor c=db.rawQuery("SELECT id,json FROM tracks WHERE id IN(SELECT track_id FROM listens WHERE id IN(SELECT id FROM active_sessions))",null)){while(c.moveToNext())if(!tracks.has(c.getString(0)))tracks.put(c.getString(0),new JSONObject(c.getString(1)));}
        try(Cursor c=db.rawQuery("SELECT l.track_id,MIN(l.started_at),SUM(l.listened_ms),SUM(l.qualified),t.json FROM listens l JOIN tracks t ON t.id=l.track_id WHERE l.started_at>=? AND l.started_at<=? AND NOT EXISTS(SELECT 1 FROM active_sessions a WHERE a.id=l.id) GROUP BY l.track_id,date(l.started_at/1000,'unixepoch','localtime')",new String[]{String.valueOf(cutoff),String.valueOf(System.currentTimeMillis())})){while(c.moveToNext()){String key=c.getString(0);if(!tracks.has(key))tracks.put(key,new JSONObject(c.getString(4)));events.put(new JSONObject().put("key",key).put("at",c.getLong(1)).put("ms",c.getLong(2)).put("plays",c.getInt(3)));}}
        db.execSQL("DROP TABLE active_sessions");result.put("tracks",tracks).put("events",events);db.setTransactionSuccessful();return result;
        }finally{db.endTransaction();}
    }
    JSONObject fullSnapshot()throws Exception{
        SQLiteDatabase db=getReadableDatabase();JSONObject result=new JSONObject();try(Cursor c=db.rawQuery("SELECT json FROM settings WHERE id='preferences'",null)){if(c.moveToFirst())result=new JSONObject(c.getString(0));}
        JSONObject tracks=new JSONObject();try(Cursor c=db.rawQuery("SELECT id,json FROM tracks",null)){while(c.moveToNext())tracks.put(c.getString(0),new JSONObject(c.getString(1)));}result.put("tracks",tracks);
        for(String table:new String[]{"listens","favorites","playlists"}){JSONArray list=new JSONArray();try(Cursor c=db.rawQuery("SELECT json FROM "+table+" ORDER BY "+(table.equals("listens")?"started_at,id":"position"),null)){while(c.moveToNext()){JSONObject row=new JSONObject(c.getString(0));row.remove("position");list.put(table.equals("favorites")?row.optString("key"):row);}}result.put(table.equals("listens")?"events":table,list);}return result;
    }
}
