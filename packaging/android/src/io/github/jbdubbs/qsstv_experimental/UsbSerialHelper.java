package io.github.jbdubbs.qsstv_experimental;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;

import java.io.IOException;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * USB serial adapters (CP210x, FTDI, CH34x, Prolific, CDC: a Digirig, a cable for a radio) for the radio side of the program, on
 * a phone that has no device nodes for them: the list of ports, the permission, the keying lines (RTS and DTR) for the PTT, and
 * a CatBridge that is the TCP port Hamlib is given. Called from src/android/androidusb.cpp through QJniObject; a port is named
 * "usb:VVVV:PPPP:N", the vendor and product number of the device and the number of the port in it (a CP2105 has two), so the
 * name that is saved in the settings is found again when the adapter is plugged in anew.
 */
public final class UsbSerialHelper
{
    private static final String TAG = "qsstv-usb";
    private static final String ACTION_PERMISSION = "io.github.jbdubbs.qsstv_experimental.USB_PERMISSION";

    /** An open port: for the keying lines, the CAT bridge, or both */
    private static final class Open implements SerialLink
    {
        UsbSerialPort port;
        UsbDeviceConnection connection;
        CatBridge bridge;

        public int read(byte[] buffer, int timeoutMs) throws IOException { return port.read(buffer, timeoutMs); }

        public void write(byte[] data, int length, int timeoutMs) throws IOException { port.write(data, length, timeoutMs); }

        public void setRts(boolean on) throws IOException { port.setRTS(on); }

        public void setDtr(boolean on) throws IOException { port.setDTR(on); }

        public void close()
        {
            if (bridge != null) bridge.stop();
            bridge = null;
            try { port.close(); } catch (IOException ignored) { }
            try { connection.close(); } catch (RuntimeException ignored) { }
        }
    }

    private static final Map<String, Open> open = new HashMap<>();
    private static String lastError = "";

    private UsbSerialHelper() { }

    private static String keyOf(UsbDevice d, int port)
    {
        return String.format(Locale.US, "usb:%04x:%04x:%d", d.getVendorId(), d.getProductId(), port);
    }

    /** The driver and the port of a key, or null */
    private static UsbSerialPort find(UsbManager um, String key)
    {
        for (UsbDevice d : um.getDeviceList().values())
        {
            UsbSerialDriver driver = UsbSerialProber.getDefaultProber().probeDevice(d);
            if (driver == null) continue;
            for (UsbSerialPort p : driver.getPorts())
                if (keyOf(d, p.getPortNumber()).equals(key)) return p;
        }
        return null;
    }

    /** One line a port: key, description, 1 when the program may use it already */
    public static String[] listPorts(Context context)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        List<String> result = new ArrayList<>();
        for (UsbDevice d : um.getDeviceList().values())
        {
            UsbSerialDriver driver = UsbSerialProber.getDefaultProber().probeDevice(d);
            if (driver == null) continue;
            String name = d.getProductName();
            if (name == null || name.isEmpty()) name = String.format(Locale.US, "USB serial %04x:%04x", d.getVendorId(), d.getProductId());
            boolean many = driver.getPorts().size() > 1;
            for (UsbSerialPort p : driver.getPorts())
                result.add(keyOf(d, p.getPortNumber()) + "\t" + name + (many ? " (port " + p.getPortNumber() + ")" : "") + "\t"
                           + (um.hasPermission(d) ? "1" : "0"));
        }
        return result.toArray(new String[0]);
    }

    public static boolean hasPermission(Context context, String key)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbSerialPort p = find(um, key);
        return p != null && um.hasPermission(p.getDriver().getDevice());
    }

    /** The system asks the person (a box over the program); hasPermission() turns true when they have allowed it */
    public static void requestPermission(Context context, String key)
    {
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbSerialPort p = find(um, key);
        if (p == null) return;
        Intent intent = new Intent(ACTION_PERMISSION).setPackage(context.getPackageName());
        int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;   // the system fills the answer in
        um.requestPermission(p.getDriver().getDevice(), PendingIntent.getBroadcast(context, 0, intent, flags));
    }

    private static Open openPort(Context context, String key) throws IOException
    {
        Open o = open.get(key);
        if (o != null) return o;
        UsbManager um = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbSerialPort port = find(um, key);
        if (port == null) throw new IOException("no such USB serial port: " + key);
        UsbDevice device = port.getDriver().getDevice();
        if (!um.hasPermission(device)) throw new IOException("no permission for " + key);
        UsbDeviceConnection connection = um.openDevice(device);
        if (connection == null) throw new IOException("the system would not open " + key);
        try
        {
            port.open(connection);
            port.setParameters(9600, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
        }
        catch (IOException | RuntimeException e)
        {
            connection.close();
            throw new IOException("cannot open " + key + ": " + e.getMessage());
        }
        o = new Open();
        o.port = port;
        o.connection = connection;
        open.put(key, o);
        return o;
    }

    /**
     * The TCP port that is the serial port, for Hamlib ("127.0.0.1:port"), or a negative number: -1 the port could not be opened
     * (lastError() says why). stopBits 1 or 2, parity 0 none 1 odd 2 even, flow 0 none 1 hardware 2 XON/XOFF.
     */
    public static synchronized int startBridge(Context context, String key, int baud, int dataBits, int stopBits, int parity, int flow)
    {
        try
        {
            Open o = openPort(context, key);
            o.port.setParameters(baud, dataBits, stopBits == 2 ? UsbSerialPort.STOPBITS_2 : UsbSerialPort.STOPBITS_1,
                                 parity == 1 ? UsbSerialPort.PARITY_ODD : parity == 2 ? UsbSerialPort.PARITY_EVEN : UsbSerialPort.PARITY_NONE);
            UsbSerialPort.FlowControl fc = flow == 1 ? UsbSerialPort.FlowControl.RTS_CTS
                    : flow == 2 ? UsbSerialPort.FlowControl.XON_XOFF : UsbSerialPort.FlowControl.NONE;
            EnumSet<UsbSerialPort.FlowControl> supported = o.port.getSupportedFlowControl();
            if (supported.contains(fc)) o.port.setFlowControl(fc);
            else Log.w(TAG, "flow control " + fc + " is not supported by " + key);
            if (o.bridge != null) o.bridge.stop();
            o.bridge = new CatBridge(o);
            int tcp = o.bridge.start();
            Log.i(TAG, "CAT bridge for " + key + " on port " + tcp);
            return tcp;
        }
        catch (IOException | RuntimeException e)
        {
            lastError = e.getMessage() == null ? e.toString() : e.getMessage();
            Log.e(TAG, "bridge for " + key + ": " + lastError);
            return -1;
        }
    }

    /** Opens the port for the keying lines (when the bridge has not), 0 or -1 */
    public static synchronized int openLines(Context context, String key)
    {
        try
        {
            openPort(context, key);
            return 0;
        }
        catch (IOException | RuntimeException e)
        {
            lastError = e.getMessage() == null ? e.toString() : e.getMessage();
            Log.e(TAG, "lines of " + key + ": " + lastError);
            return -1;
        }
    }

    /** Sets the lines of an open port; -1 leaves a line as it is, 0 off, 1 on */
    public static synchronized boolean setLines(String key, int rts, int dtr)
    {
        Open o = open.get(key);
        if (o == null) return false;
        try
        {
            if (rts >= 0) o.port.setRTS(rts == 1);
            if (dtr >= 0) o.port.setDTR(dtr == 1);
            return true;
        }
        catch (IOException | RuntimeException e)
        {
            lastError = e.getMessage() == null ? e.toString() : e.getMessage();
            return false;
        }
    }

    /** Stops the bridge and closes the port */
    public static synchronized void close(String key)
    {
        Open o = open.remove(key);
        if (o != null) o.close();
    }

    public static synchronized String lastError()
    {
        return lastError;
    }
}
