package io.github.jbdubbs.qsstv_experimental;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.drawable.Icon;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

/**
 * Keeps the program receiving and transmitting with the screen off or another app in front.
 *
 * Android silences the microphone of an app that is not visible, and may stop its process, unless the app runs a
 * foreground service of type "microphone" (declared in the manifest). The C++ side calls setActive(true) while the sound
 * backend is capturing or playing and setActive(false) when it is idle, so the service, and its ongoing notification,
 * exist only while there is something to receive or send. It also holds a partial wake lock, so the CPU keeps decoding
 * with the screen off.
 *
 * The notification says which of the two is going on (listening, sending) and has a Stop button: it stops what runs (the picture that is
 * sent, otherwise the receiver) through the program (stopRequested, src/android/androidshell.cpp), and the service then ends by itself.
 */
public class RxService extends Service
{
    private static final String TAG = "qsstv-service";
    private static final String CHANNEL = "receiving";
    private static final int NOTIFICATION_ID = 1;
    private static final String ACTION_STOP = "io.github.jbdubbs.qsstv_experimental.STOP";
    private static boolean running = false;
    private PowerManager.WakeLock wakeLock;

    /** The Stop button was pressed. Implemented in src/android/androidshell.cpp. */
    private static native void stopRequested();

    /**
     * Called from the program (QJniObject) in its GUI thread: 0 nothing to do (the service ends), 1 listening, 2 sending a picture, 3 sending a tone or text (detail: what
     * is sent, for the notification). The app must be in the foreground when it starts the service (the user pressed Start).
     */
    public static void setState(Context context, int state, String detail)
    {
        try
        {
            if (state == 0)
            {
                context.stopService(new Intent(context, RxService.class));
            }
            else if (running)
            {
                // the service is there: the notification changes, nothing is started again
                createChannel(context);
                ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                    .notify(NOTIFICATION_ID, buildNotification(context, state, detail));
            }
            else
            {
                Intent intent = new Intent(context, RxService.class);
                intent.putExtra("state", state);
                intent.putExtra("detail", detail);
                context.startForegroundService(intent);
            }
        }
        catch (RuntimeException e)
        {
            // a start refused because the app is not allowed to start a foreground service now (it is in the background)
            Log.w(TAG, "setState(" + state + ") failed: " + e);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        if (intent != null && ACTION_STOP.equals(intent.getAction()))
        {
            try
            {
                stopRequested();   // the program stops what it is doing; its next look at the sound device ends this service
            }
            catch (UnsatisfiedLinkError e)
            {
                Log.w(TAG, "the program is not there to stop: " + e);
                stopSelf();
            }
            return START_NOT_STICKY;
        }
        createChannel(this);
        int state = intent != null ? intent.getIntExtra("state", 1) : 1;
        String detail = intent != null ? intent.getStringExtra("detail") : null;
        Notification notification = buildNotification(this, state, detail);
        try
        {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
        }
        catch (RuntimeException e)
        {
            // for instance the microphone permission is missing (SecurityException): nothing to keep alive
            Log.w(TAG, "startForeground failed: " + e);
            stopSelf();
            return START_NOT_STICKY;
        }
        if (wakeLock == null)
        {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "qsstv:receiving");
            wakeLock.setReferenceCounted(false);
            wakeLock.acquire();
        }
        running = true;
        Log.i(TAG, "foreground service started");
        return START_NOT_STICKY;   // if the system kills the process, the program is not silently restarted in the background
    }

    @Override
    public void onDestroy()
    {
        running = false;
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        wakeLock = null;
        Log.i(TAG, "foreground service stopped");
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }

    /** A string of res/values/strings.xml by name (the R class of the package is not known here: the manifest's package is a placeholder) */
    private static String str(Context context, String name)
    {
        int id = context.getResources().getIdentifier(name, "string", context.getPackageName());
        return id != 0 ? context.getString(id) : name;
    }

    private static void createChannel(Context context)
    {
        NotificationManager nm = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        // IMPORTANCE_LOW: shown in the shade, no sound
        NotificationChannel channel = new NotificationChannel(CHANNEL, str(context, "notification_channel"), NotificationManager.IMPORTANCE_LOW);
        channel.setDescription(str(context, "notification_channel_description"));
        nm.createNotificationChannel(channel);
    }

    private static Notification buildNotification(Context context, int state, String detail)
    {
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        PendingIntent open = PendingIntent.getActivity(context, 0, launch, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Intent stop = new Intent(context, RxService.class).setAction(ACTION_STOP);
        PendingIntent stopIntent = PendingIntent.getService(context, 1, stop, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        int icon = context.getResources().getIdentifier("ic_stat_rx", "drawable", context.getPackageName());
        boolean sending = state >= 2;
        String text = str(context, sending ? "notification_sending_text" : "notification_listening_text");
        if (sending && detail != null && !detail.isEmpty()) text = detail + ". " + text;
        String title = str(context, state == 2 ? "notification_sending" : state == 3 ? "notification_sending_other" : "notification_listening");
        Notification.Action.Builder action = new Notification.Action.Builder(
            Icon.createWithResource(context, icon != 0 ? icon : android.R.drawable.ic_btn_speak_now), str(context, "notification_stop"), stopIntent);
        return new Notification.Builder(context, CHANNEL)
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(icon != 0 ? icon : android.R.drawable.ic_btn_speak_now)
                .setContentIntent(open)
                .addAction(action.build())
                .setOngoing(true)
                .setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
                .build();
    }
}
