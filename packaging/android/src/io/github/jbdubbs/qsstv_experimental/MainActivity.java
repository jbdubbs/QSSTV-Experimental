package io.github.jbdubbs.qsstv_experimental;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * The program's activity: Qt's own, with one addition. A picture shared to the program from another app arrives as ACTION_SEND with
 * the picture in EXTRA_STREAM; Qt only hands on the data of an ACTION_VIEW intent ("Open with"), so a share is turned into one
 * before Qt looks at it, and the rest of the program treats both alike.
 *
 * It also carries what the program needs of the system's shell: the launcher shortcuts (the screen they ask for is kept until the program
 * takes it, takeRequestedScreen) and the Back of Android 13 and later (predictive Back): a callback is registered for the life of the activity, and
 * the program decides what Back does, a step back or, from the first screen, the home screen. Without a callback the system finishes the activity
 * when Back is pressed on the first screen, which would end the receiving.
 */
public class MainActivity extends QtActivity
{
    private static final String TAG = "qsstv-files";
    private static final int REQUEST_PHOTO = 0x5153;

    /** The system's Back (gesture or button) arrived: the program goes back one step, or to the home screen. Implemented in src/android/androidshell.cpp. */
    private static native void backInvoked();

    private static String requestedScreen;

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
        noteScreen(getIntent());
        super.onCreate(savedInstanceState);
        registerBack();
    }

    @Override
    public void onNewIntent(Intent intent)
    {
        noteScreen(intent);
        super.onNewIntent(shareAsView(intent));
    }

    /**
     * With the callback of Android 13 and later Back is the callback's business: a Back key event (the button of the three-button navigation)
     * would otherwise be handled twice, by Qt as a key and by the callback.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event)
    {
        if (Build.VERSION.SDK_INT >= 33 && event.getKeyCode() == KeyEvent.KEYCODE_BACK) return false;
        return super.dispatchKeyEvent(event);
    }

    /**
     * The activity ends (removed from the recent apps, or finished by the system): Qt cannot end its application then. Its camera listener waits
     * for the UI thread while the UI thread, in Qt's onDestroy, waits for Qt, and the process hangs with a notification that does nothing. Without
     * its activity the program cannot receive anyway, so the process ends here, and the service with it.
     */
    @Override
    protected void onDestroy()
    {
        if (isFinishing())
        {
            stopService(new Intent(this, RxService.class));
            android.os.Process.killProcess(android.os.Process.myPid());
        }
        super.onDestroy();
    }

    /** A launcher shortcut asks for a screen: "receive", "transmit" or "gallery" (res/xml/shortcuts.xml). Kept until the program takes it. */
    private static void noteScreen(Intent intent)
    {
        if (intent == null) return;
        String screen = intent.getStringExtra("qsstv.screen");
        if (screen == null) return;
        intent.removeExtra("qsstv.screen");   // not again when the activity is recreated from the same intent
        requestedScreen = screen;
        Log.i(TAG, "shortcut: " + screen);
    }

    /** Called by the program (QJniObject): the screen a shortcut asked for, once; null when there is none. */
    public static String takeRequestedScreen()
    {
        String s = requestedScreen;
        requestedScreen = null;
        return s;
    }

    private void registerBack()
    {
        if (Build.VERSION.SDK_INT < 33) return;   // before: Back arrives as a key and Qt takes it (the window's onClosing)
        OnBackInvokedCallback callback = () -> {
            try
            {
                backInvoked();
            }
            catch (UnsatisfiedLinkError e)
            {
                moveTaskToBack(true);   // the program is not up yet: Back leaves it alone
            }
        };
        getOnBackInvokedDispatcher().registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
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
