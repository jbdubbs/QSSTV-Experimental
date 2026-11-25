package io.github.jbdubbs.qsstv_experimental;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * A USB sound-card interface with a CM108, CM119 or compatible chip, keyed through a GPIO pin (rig/cm108lines.cpp is the desktop
 * twin). A phone has no /dev/hidraw for an app: the pin is set with a HID SET_REPORT control transfer to the HID interface of the
 * device, the same 4 bytes that Linux writes to hidraw. The audio interfaces of the device are not touched, so the sound goes on
 * through the audio path of the program. Called from src/android/androidusb.cpp through QJniObject; a device is named
 * "hid:VVVV:PPPP:N", the vendor and product number and its place among the devices that have the same numbers.
 */
public final class Cm108Helper
{
    private static final String TAG = "qsstv-cm108";
    private static final String ACTION_PERMISSION = "io.github.jbdubbs.qsstv_experimental.USB_PERMISSION";

    private static final class Open
    {
        UsbDeviceConnection connection;
        UsbInterface iface;
    }

    private static final Map<String, Open> open = new HashMap<>();
    private static String lastError = "";

    private Cm108Helper() { }

    /** The ids of the chips that have the GPIO report: the same table as Hamlib and cm108Ptt::compatible() */
    static boolean compatible(int vendor, int product)
    {
        return (vendor == 0x0d8c && ((product >= 0x0008 && product <= 0x000f) || product == 0x0012 || product == 0x013a
                                     || product == 0x013c || product == 0x0013))
               || (vendor == 0x0c76 && (product == 0x1605 || product == 0x1607 || product == 0x160b));
    }

    private static UsbInterface hidInterface(UsbDevice d)
    {
        for (int i = 0; i < d.getInterfaceCount(); i++)
            if (d.getInterface(i).getInterfaceClass() == UsbConstants.USB_CLASS_HID) return d.getInterface(i);
        return null;
    }

    private static String keyOf(UsbDevice d, int n)
    {
        return String.format(Locale.US, "hid:%04x:%04x:%d", d.getVendorId(), d.getProductId(), n);
    }

    /** The CM108 devices that are attached, with their keys, in a fixed order */
    private static Map<String, UsbDevice> devices(UsbManager um)
    {
        List<UsbDevice> found = new ArrayList<>();
        for (UsbDevice d : um.getDeviceList().values())
            if (compatible(d.getVendorId(), d.getProductId()) && hidInterface(d) != null) found.add(d);
        Collections.sort(found, (a, b) -> a.getDeviceName().compareTo(b.getDeviceName()));
        Map<String, UsbDevice> result = new java.util.LinkedHashMap<>();
        Map<String, Integer> count = new HashMap<>();
        for (UsbDevice d : found)
        {
            String ids = String.format(Locale.US, "%04x:%04x", d.getVendorId(), d.getProductId());
            int n = count.containsKey(ids) ? count.get(ids) : 0;
            count.put(ids, n + 1);
            result.put(keyOf(d, n), d);
        }
        return result;
    }

    /** One line a device: key, description, 1 when the program may use it already */
    public static String[] list(Context context)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        List<String> result = new ArrayList<>();
        for (Map.Entry<String, UsbDevice> e : devices(um).entrySet())
        {
            UsbDevice d = e.getValue();
            String name = d.getProductName();
            if (name == null || name.isEmpty()) name = "CM108";
            result.add(e.getKey() + "\t" + name + " (" + e.getKey().substring(4, 13) + ")\t" + (um.hasPermission(d) ? "1" : "0"));
        }
        return result.toArray(new String[0]);
    }

    public static boolean hasPermission(Context context, String key)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbDevice d = devices(um).get(key);
        return d != null && um.hasPermission(d);
    }

    /** The system asks the person (a box over the program); hasPermission() turns true when they have allowed it */
    public static void requestPermission(Context context, String key)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbDevice d = devices(um).get(key);
        if (d == null) return;
        Intent intent = new Intent(ACTION_PERMISSION).setPackage(context.getPackageName());
        int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;   // the system fills the answer in
        um.requestPermission(d, PendingIntent.getBroadcast(context, 0, intent, flags));
    }

    /** Takes the HID interface of the device (a kernel driver that has it is detached), 0 or -1 with lastError() */
    public static synchronized int open(Context context, String key)
    {
        if (open.containsKey(key)) return 0;
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbDevice d = devices(um).get(key);
        if (d == null) return fail("no such CM108 device: " + key);
        if (!um.hasPermission(d)) return fail("no permission for " + key);
        UsbInterface iface = hidInterface(d);
        UsbDeviceConnection connection = um.openDevice(d);
        if (connection == null) return fail("the system would not open " + key);
        if (!connection.claimInterface(iface, true))
        {
            connection.close();
            return fail("another program has the HID interface of " + key);
        }
        Open o = new Open();
        o.connection = connection;
        o.iface = iface;
        open.put(key, o);
        return 0;
    }

    /** One HID SET_REPORT (output report 0) with the 4 bytes of the CM108 "write GPIO" report */
    public static synchronized boolean send(String key, int b0, int b1, int b2, int b3)
    {
        Open o = open.get(key);
        if (o == null) return false;
        byte[] data = {(byte) b0, (byte) b1, (byte) b2, (byte) b3};
        // bmRequestType 0x21 (class, interface, host to device), bRequest 0x09 SET_REPORT, wValue (output report) << 8 | report ID 0
        int r = o.connection.controlTransfer(0x21, 0x09, 0x0200, o.iface.getId(), data, data.length, 1000);
        if (r < 0)
        {
            lastError = "the device did not take the report (unplugged?)";
            Log.e(TAG, lastError);
            return false;
        }
        return true;
    }

    public static synchronized void close(String key)
    {
        Open o = open.remove(key);
        if (o == null) return;
        try { o.connection.releaseInterface(o.iface); } catch (RuntimeException ignored) { }
        try { o.connection.close(); } catch (RuntimeException ignored) { }
    }

    private static int fail(String why)
    {
        lastError = why;
        Log.e(TAG, why);
        return -1;
    }

    public static synchronized String lastError()
    {
        return lastError;
    }
}
