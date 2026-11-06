package io.github.jbdubbs.qsstv_experimental;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * The program's activity: Qt's own, with one addition. A picture shared to the program from another app arrives as ACTION_SEND with
 * the picture in EXTRA_STREAM; Qt only hands on the data of an ACTION_VIEW intent ("Open with"), so a share is turned into one
 * before Qt looks at it, and the rest of the program treats both alike.
 */
public class MainActivity extends QtActivity
{
    private static final String TAG = "qsstv-files";
    private static final int REQUEST_PHOTO = 0x5153;

    /** The photo the user picked, as a content:// URI string (the program copies it into its cache). Implemented in src/android/photopicker.cpp. */
    private static native void photoPicked(String uri);

    /**
     * The system's photo picker (Android 13 and later: a browsable library of the user's photos, no permission asked), or the
     * system's image chooser before that. The result comes back to onActivityResult().
     */
    public void pickPhoto()
    {
        Intent intent;
        if (Build.VERSION.SDK_INT >= 33)
        {
            intent = new Intent(MediaStore.ACTION_PICK_IMAGES);
        }
        else
        {
            intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
        }
        intent.setType("image/*");
        try
        {
            startActivityForResult(intent, REQUEST_PHOTO);
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "no photo picker: " + e);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        if (requestCode != REQUEST_PHOTO)
        {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        if (resultCode == RESULT_OK && data != null && data.getData() != null)
        {
            Log.i(TAG, "photo picked: " + data.getData());
            photoPicked(data.getData().toString());
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        setIntent(shareAsView(getIntent()));
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onNewIntent(Intent intent)
    {
        super.onNewIntent(shareAsView(intent));
    }

    @SuppressWarnings("deprecation")
    private static Intent shareAsView(Intent intent)
    {
        if (intent == null || !Intent.ACTION_SEND.equals(intent.getAction())) return intent;
        Uri stream = Build.VERSION.SDK_INT >= 33 ? intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri.class)
                                                 : (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
        if (stream == null) return intent;
        Log.i(TAG, "shared: " + stream + " (" + intent.getType() + ")");
        intent.setAction(Intent.ACTION_VIEW);
        intent.setDataAndType(stream, intent.getType());
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return intent;
    }
}
