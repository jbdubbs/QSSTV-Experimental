package io.github.jbdubbs.qsstv_experimental;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;

/**
 * Hamlib on Android cannot open a USB serial adapter (no device node, no libusb): it is given a TCP port instead, and this
 * is what is behind the port: whatever Hamlib sends goes to the serial line, whatever the radio answers comes back.
 * Hamlib takes a "host:port" for a serial radio as a network port (rig.c: the address replaces the port type), and its
 * serial backends talk through it as they do through a cable.
 *
 * One connection at a time, on the loopback address only. Another program on the same phone could connect to the port while
 * it is open (Android has no way to keep a loopback port to one app), and so could send the radio commands: the port is
 * opened when CAT is started and closed when it is stopped, and a second connection is refused while the first is up.
 */
public class CatBridge
{
    private final SerialLink link;
    private ServerSocket server;
    private volatile boolean running;
    private volatile Socket client;
    private Thread acceptThread;

    public CatBridge(SerialLink link)
    {
        this.link = link;
    }

    /** Opens the port and returns its number */
    public synchronized int start() throws IOException
    {
        server = new ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"));
        running = true;
        acceptThread = new Thread(this::acceptLoop, "cat-bridge-accept");
        acceptThread.setDaemon(true);
        acceptThread.start();
        return server.getLocalPort();
    }

    public synchronized void stop()
    {
        running = false;
        try { if (server != null) server.close(); } catch (IOException ignored) { }
        Socket c = client;
        try { if (c != null) c.close(); } catch (IOException ignored) { }
    }

    private void acceptLoop()
    {
        while (running)
        {
            try
            {
                Socket s = server.accept();
                if (client != null && !client.isClosed())
                {
                    s.close();   // one at a time
                    continue;
                }
                client = s;
                s.setTcpNoDelay(true);
                // a thread of its own, so that the next connection is accepted (and refused) while this one is served
                Thread t = new Thread(() ->
                {
                    serve(s);
                    client = null;
                }, "cat-bridge-serve");
                t.setDaemon(true);
                t.start();
            }
            catch (IOException e)
            {
                if (!running) return;
            }
        }
    }

    private void serve(Socket s)
    {
        // what the program sends to the radio: a thread of its own, the radio's answers are read here
        Thread toRadio = new Thread(() ->
        {
            byte[] buf = new byte[512];
            try
            {
                InputStream in = s.getInputStream();
                int n;
                while (running && (n = in.read(buf)) >= 0) link.write(buf, n, 1000);
            }
            catch (IOException ignored) { }
            try { s.close(); } catch (IOException ignored) { }
        }, "cat-bridge-to-radio");
        toRadio.setDaemon(true);
        toRadio.start();
        byte[] buf = new byte[512];
        try
        {
            OutputStream out = s.getOutputStream();
            while (running && !s.isClosed())
            {
                int n = link.read(buf, 50);
                if (n > 0)
                {
                    out.write(buf, 0, n);
                    out.flush();
                }
            }
        }
        catch (IOException ignored) { }
        try { s.close(); } catch (IOException ignored) { }
    }
}
