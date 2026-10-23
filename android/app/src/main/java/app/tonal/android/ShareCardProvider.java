package app.tonal.android;

import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.*;

/** Grants expose only generated PNG cards, never library files or credentials. */
public final class ShareCardProvider extends ContentProvider {
    public boolean onCreate(){return true;}
    private File card(Uri u)throws FileNotFoundException{
        if(!u.getPath().matches("/[a-f0-9]{32}\\.png"))throw new FileNotFoundException();
        File f=new File(new File(getContext().getCacheDir(),"share-cards"),u.getLastPathSegment());
        if(!f.isFile())throw new FileNotFoundException();return f;
    }
    public String getType(Uri u){return "image/png";}
    public Cursor query(Uri u,String[] p,String s,String[] a,String sort){
        try{File f=card(u);String[] columns=p==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:p;
            MatrixCursor result=new MatrixCursor(columns);Object[] row=new Object[columns.length];for(int i=0;i<columns.length;i++)row[i]=columns[i].equals(OpenableColumns.DISPLAY_NAME)?"Tonalify-musica.png":columns[i].equals(OpenableColumns.SIZE)?f.length():null;result.addRow(row);return result;
        }catch(FileNotFoundException e){return null;}
    }
    public ParcelFileDescriptor openFile(Uri u,String mode)throws FileNotFoundException{if(!"r".equals(mode))throw new FileNotFoundException();return ParcelFileDescriptor.open(card(u),ParcelFileDescriptor.MODE_READ_ONLY);}
    public Uri insert(Uri u,ContentValues v){throw new UnsupportedOperationException();}
    public int update(Uri u,ContentValues v,String s,String[] a){throw new UnsupportedOperationException();}
    public int delete(Uri u,String s,String[] a){throw new UnsupportedOperationException();}
}
