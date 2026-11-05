package io.github.jbdubbs.qsstv_experimental;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
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
 */
public class RxService extends Service
{
    private static final String TAG = "qsstv-service";
    private static final String CHANNEL = "receiving";
    private static final int NOTIFICATION_ID = 1;
    private PowerManager.WakeLock wakeLock;

    /** Called from the program (QJniObject) in its GUI thread. The app must be in the foreground (the user pressed Start). */
    public static void setActive(Context context, boolean on)
    {
        try
        {
            Intent intent = new Intent(context, RxService.class);
            if (on) context.startForegroundService(intent);
            else context.stopService(intent);
        }
        catch (RuntimeException e)
        {
            // a start refused because the app is not allowed to start a foreground service now (it is in the background)
            Log.w(TAG, "setActive(" + on + ") failed: " + e);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        createChannel();
        Notification notification = buildNotification();
        try
        {
            if (Build.VERSION.SDK_INT >= 29)
                startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
            else
                startForeground(NOTIFICATION_ID, notification);
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
        Log.i(TAG, "foreground service started");
        return START_NOT_STICKY;   // if the system kills the process, the program is not silently restarted in the background
    }

    @Override
    public void onDestroy()
    {
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

    private void createChannel()
    {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        // IMPORTANCE_LOW: shown in the shade, no sound
        NotificationChannel channel = new NotificationChannel(CHANNEL, "Receiving and transmitting", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Shown while QSSTV-Experimental is listening or sending");
        nm.createNotificationChannel(channel);
    }

    private Notification buildNotification()
    {
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent open = PendingIntent.getActivity(this, 0, launch, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        int icon = getResources().getIdentifier("ic_stat_rx", "drawable", getPackageName());
        return new Notification.Builder(this, CHANNEL)
                .setContentTitle("QSSTV-Experimental")
                .setContentText("Listening for pictures")
                .setSmallIcon(icon != 0 ? icon : android.R.drawable.ic_btn_speak_now)
                .setContentIntent(open)
                .setOngoing(true)
                .setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
                .build();
    }
}
