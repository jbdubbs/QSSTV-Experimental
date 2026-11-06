package io.github.jbdubbs.qsstv_experimental;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.util.Log;

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * The two file jobs Qt cannot do by itself on Android, called from src/utils/platformfiles.cpp:
 * the real name of a file the user picked (the picker returns a content:// URI), and putting a received picture where the
 * phone's gallery shows it (MediaStore, Pictures/QSSTV-Experimental).
 */
public class FileHelper
{
    private static final String TAG = "qsstv-files";

    /** The name the picker shows for the document, with its extension, or the last part of the URI */
    public static String displayName(Context context, String uriString)
    {
        Uri uri = Uri.parse(uriString);
        try (Cursor c = context.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null))
        {
            if (c != null && c.moveToFirst())
            {
                String name = c.getString(0);
                if (name != null && !name.isEmpty()) return name;
            }
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "displayName: " + e);
        }
        String last = uri.getLastPathSegment();
        return last != null ? last : "file";
    }

    /**
     * Hand a picture to another app through the system share sheet. The file is in the app's own storage, so it goes out as
     * a content:// URI from Qt's FileProvider (declared in the manifest, authority "<application id>.qtprovider", which covers
     * the app's files folder) with a read grant for the receiving app.
     */
    public static void shareImage(Context context, String path)
    {
        try
        {
            java.io.File file = new java.io.File(path);
            Uri uri = androidx.core.content.FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider", file);
            String mime = imageMimeType(file.getName());
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType(mime != null ? mime : "image/*");
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            Intent chooser = Intent.createChooser(send, "Share picture");
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(chooser);
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "shareImage " + path + ": " + e);
        }
    }

    private static String imageMimeType(String name)
    {
        String n = name.toLowerCase(Locale.ROOT);
        if (n.endsWith(".png")) return "image/png";
        if (n.endsWith(".jpg") || n.endsWith(".jpeg")) return "image/jpeg";
        if (n.endsWith(".webp")) return "image/webp";
        if (n.endsWith(".bmp")) return "image/bmp";
        if (n.endsWith(".gif")) return "image/gif";
        return null;   // JPEG 2000, AVIF and the like: no gallery entry
    }

    /**
     * Copy a picture into the shared Pictures/QSSTV-Experimental folder. Returns the MediaStore URI, or "" when nothing was
     * done: not a picture type the gallery knows, or an error.
     */
    public static String saveToGallery(Context context, String path, String displayName)
    {
        String mime = imageMimeType(displayName);
        if (mime == null) return "";
        ContentResolver resolver = context.getContentResolver();
        ContentValues values = new ContentValues();
        values.put(MediaStore.Images.Media.DISPLAY_NAME, displayName);
        values.put(MediaStore.Images.Media.MIME_TYPE, mime);
        values.put(MediaStore.Images.Media.RELATIVE_PATH, Environment.DIRECTORY_PICTURES + "/QSSTV-Experimental");
        values.put(MediaStore.Images.Media.IS_PENDING, 1);   // invisible until the bytes are in
        Uri uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values);
        if (uri == null) return "";
        try (InputStream in = new FileInputStream(path); OutputStream out = resolver.openOutputStream(uri))
        {
            if (out == null) throw new java.io.IOException("no output stream");
            byte[] buffer = new byte[65536];
            int n;
            while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
        }
        catch (Exception e)
        {
            Log.w(TAG, "saveToGallery " + path + ": " + e);
            resolver.delete(uri, null, null);
            return "";
        }
        values.clear();
        values.put(MediaStore.Images.Media.IS_PENDING, 0);
        resolver.update(uri, values, null, null);
        return uri.toString();
    }
}
