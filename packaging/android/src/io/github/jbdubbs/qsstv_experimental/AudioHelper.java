package io.github.jbdubbs.qsstv_experimental;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * What the Android audio manager knows and Qt does not tell: the devices with their product names, the media volume, audio
 * focus while transmitting, and the two permission screens. Called from src/android/androidaudio.cpp through QJniObject.
 */
public class AudioHelper
{
    private static final String TAG = "qsstv-audio";
    private static AudioFocusRequest focusRequest;

    private static String typeName(int type)
    {
        switch (type)
        {
        case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE: return "Phone earpiece";
        case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER: return "Phone speaker";
        case AudioDeviceInfo.TYPE_BUILTIN_MIC: return "Phone microphone";
        case AudioDeviceInfo.TYPE_WIRED_HEADSET: return "Wired headset";
        case AudioDeviceInfo.TYPE_WIRED_HEADPHONES: return "Wired headphones";
        case AudioDeviceInfo.TYPE_LINE_ANALOG:
        case AudioDeviceInfo.TYPE_LINE_DIGITAL: return "Line";
        case AudioDeviceInfo.TYPE_USB_DEVICE: return "USB audio";
        case AudioDeviceInfo.TYPE_USB_HEADSET: return "USB headset";
        case AudioDeviceInfo.TYPE_USB_ACCESSORY: return "USB accessory";
        case AudioDeviceInfo.TYPE_BLUETOOTH_SCO: return "Bluetooth calls (narrowband)";
        case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP: return "Bluetooth";
        case AudioDeviceInfo.TYPE_HDMI: return "HDMI";
        case AudioDeviceInfo.TYPE_DOCK: return "Dock";
        case AudioDeviceInfo.TYPE_REMOTE_SUBMIX: return "Remote submix";
        case AudioDeviceInfo.TYPE_TELEPHONY: return "Telephony";
        default: return "Audio device " + type;
        }
    }

    /**
     * The audio inputs or outputs now present, one string per device: id, type, name shown to the user, a flag that is 1 for a
     * device that is part of the phone itself, tab separated. The name is the kind of device, plus the product name where
     * that tells devices of the same kind apart (USB and Bluetooth): "USB audio: Digirig Mobile". Two of the same name get a
     * number, so a saved choice finds the same device again after it is plugged in anew (its id changes).
     */
    public static String[] listDevices(Context context, boolean inputs)
    {
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        AudioDeviceInfo[] devices = am.getDevices(inputs ? AudioManager.GET_DEVICES_INPUTS : AudioManager.GET_DEVICES_OUTPUTS);
        List<String> result = new ArrayList<>();
        List<String> names = new ArrayList<>();
        for (AudioDeviceInfo d : devices)
        {
            int type = d.getType();
            boolean builtIn = type == AudioDeviceInfo.TYPE_BUILTIN_EARPIECE || type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
                    || type == AudioDeviceInfo.TYPE_BUILTIN_MIC || (Build.VERSION.SDK_INT >= 30 && type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE);
            String name = typeName(type);
            CharSequence product = d.getProductName();
            if (!builtIn && product != null && product.length() > 0) name += ": " + product;
            String unique = name;
            for (int n = 2; names.contains(unique); n++) unique = name + " (" + n + ")";
            names.add(unique);
            result.add(d.getId() + "\t" + type + "\t" + unique + "\t" + (builtIn ? "1" : "0"));
        }
        return result.toArray(new String[0]);
    }

    /** The media volume, 0 to 100 */
    public static int musicVolumePercent(Context context)
    {
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        int max = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        return max > 0 ? Math.round(100f * am.getStreamVolume(AudioManager.STREAM_MUSIC) / max) : 0;
    }

    public static void setMusicVolumePercent(Context context, int percent)
    {
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        int max = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        am.setStreamVolume(AudioManager.STREAM_MUSIC, Math.round(max * Math.max(0, Math.min(100, percent)) / 100f), 0);
    }

    /**
     * Ask the other apps to stop playing while a transmission is on the speaker: music pauses, and a notification does not
     * sound over the picture. Nothing is asked for while receiving.
     */
    public static synchronized void requestTransmitFocus(Context context)
    {
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (focusRequest == null)
        {
            AudioAttributes attributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build();
            focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)
                    .setAudioAttributes(attributes)
                    .setWillPauseWhenDucked(false)
                    .setOnAudioFocusChangeListener(change -> Log.i(TAG, "audio focus change " + change))
                    .build();
        }
        int r = am.requestAudioFocus(focusRequest);
        Log.i(TAG, "transmit audio focus " + (r == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? "granted" : "refused (" + r + ")"));
    }

    public static synchronized void abandonTransmitFocus(Context context)
    {
        if (focusRequest == null) return;
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        am.abandonAudioFocusRequest(focusRequest);
    }

    /** Android 13 and later: without this the foreground service runs, but its notification is not shown */
    public static void requestNotificationPermission(Context context)
    {
        if (Build.VERSION.SDK_INT < 33 || !(context instanceof Activity)) return;
        Activity activity = (Activity) context;
        if (activity.checkSelfPermission("android.permission.POST_NOTIFICATIONS") == android.content.pm.PackageManager.PERMISSION_GRANTED) return;
        activity.requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 4711);
    }

    /** The system's screen for this app, where a permission that was refused can be given again */
    public static void openAppSettings(Context context)
    {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", context.getPackageName(), null));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}
