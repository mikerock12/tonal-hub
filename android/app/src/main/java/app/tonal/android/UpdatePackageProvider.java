package app.tonal.android;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;

/** The installer receives read access only to the verified staged APK. */
public final class UpdatePackageProvider extends ContentProvider {
    public boolean onCreate() { return true; }
    private File apk(Uri uri) throws FileNotFoundException {
        if (!("/" + AppUpdate.APK).equals(uri.getPath())) throw new FileNotFoundException();
        File file = new File(getContext().getCacheDir(), AppUpdate.APK);
        if (!file.isFile()) throw new FileNotFoundException();
        return file;
    }
    public String getType(Uri uri) { return "application/vnd.android.package-archive"; }
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) throw new FileNotFoundException();
        return ParcelFileDescriptor.open(apk(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
        try {
            File file = apk(uri);
            String[] columns = projection == null ? new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE} : projection;
            MatrixCursor cursor = new MatrixCursor(columns);
            MatrixCursor.RowBuilder row = cursor.newRow();
            for (String column : columns)
                row.add(OpenableColumns.SIZE.equals(column) ? file.length() : OpenableColumns.DISPLAY_NAME.equals(column) ? AppUpdate.APK : null);
            return cursor;
        } catch (FileNotFoundException e) { return null; }
    }
    public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    public int delete(Uri uri, String selection, String[] args) { throw new UnsupportedOperationException(); }
    public int update(Uri uri, ContentValues values, String selection, String[] args) { throw new UnsupportedOperationException(); }
}
