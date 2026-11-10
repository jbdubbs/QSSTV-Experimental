import io.github.jbdubbs.qsstv_experimental.CatBridge;
import io.github.jbdubbs.qsstv_experimental.SerialLink;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/** The CAT bridge between a TCP client (what Hamlib is) and a serial line (here a radio of a few lines): commands go, answers come back. */
public class CatBridgeTest
{
    static int failures = 0;

    static void check(boolean ok, String what)
    {
        System.out.println("  " + (ok ? "ok    " : "FAIL  ") + what);
        if (!ok) failures++;
    }

    /** A Kenwood-style radio: ID; FA; and a PTT line that is recorded */
    static class FakeRadio implements SerialLink
    {
        final ArrayBlockingQueue<byte[]> answers = new ArrayBlockingQueue<>(100);
        final ByteArrayOutputStream received = new ByteArrayOutputStream();
        final StringBuilder pending = new StringBuilder();
        volatile boolean rts, dtr, closed;

        public int read(byte[] buffer, int timeoutMs) throws IOException
        {
            try
            {
                byte[] a = answers.poll(timeoutMs, TimeUnit.MILLISECONDS);
                if (a == null) return 0;
                System.arraycopy(a, 0, buffer, 0, a.length);
                return a.length;
            }
            catch (InterruptedException e) { return 0; }
        }

        public void write(byte[] data, int length, int timeoutMs)
        {
            received.write(data, 0, length);
            for (int i = 0; i < length; i++)
            {
                char c = (char) data[i];
                if (c != ';') { pending.append(c); continue; }
                String cmd = pending.toString();
                pending.setLength(0);
                if (cmd.equals("ID")) answers.add("ID020;".getBytes());
                else if (cmd.equals("FA")) answers.add("FA00014250000;".getBytes());
            }
        }

        public void setRts(boolean on) { rts = on; }
        public void setDtr(boolean on) { dtr = on; }
        public void close() { closed = true; }
    }

    static String readUntil(InputStream in, char end, int ms) throws IOException
    {
        StringBuilder sb = new StringBuilder();
        long until = System.currentTimeMillis() + ms;
        while (System.currentTimeMillis() < until)
        {
            if (in.available() > 0)
            {
                char c = (char) in.read();
                sb.append(c);
                if (c == end) break;
            }
            else try { Thread.sleep(5); } catch (InterruptedException ignored) { }
        }
        return sb.toString();
    }

    public static void main(String[] args) throws Exception
    {
        FakeRadio radio = new FakeRadio();
        CatBridge bridge = new CatBridge(radio);
        int port = bridge.start();
        check(port > 0, "the bridge listens on a port (" + port + ")");

        Socket hamlib = new Socket("127.0.0.1", port);
        OutputStream out = hamlib.getOutputStream();
        InputStream in = hamlib.getInputStream();
        out.write("ID;".getBytes());
        out.flush();
        check(readUntil(in, ';', 2000).equals("ID020;"), "a command goes to the radio and its answer comes back");
        out.write("FA;".getBytes());
        out.flush();
        check(readUntil(in, ';', 2000).equals("FA00014250000;"), "and a second one");
        out.write("FA".getBytes());
        out.flush();
        Thread.sleep(50);
        out.write(";".getBytes());
        out.flush();
        check(readUntil(in, ';', 2000).equals("FA00014250000;"), "a command that arrives in two pieces is one command to the radio");
        check(new String(radio.received.toByteArray()).equals("ID;FA;FA;"), "the radio saw exactly what was sent");

        Socket second = new Socket("127.0.0.1", port);
        Thread.sleep(150);
        boolean refused = false;
        try { second.getOutputStream().write("ID;".getBytes()); second.getOutputStream().flush(); refused = second.getInputStream().read() < 0; }
        catch (IOException e) { refused = true; }
        check(refused, "a second connection is refused while the first is up");
        check(radio.received.size() == 9, "and nothing of it reached the radio");

        hamlib.close();
        Thread.sleep(150);
        Socket again = new Socket("127.0.0.1", port);
        again.getOutputStream().write("ID;".getBytes());
        again.getOutputStream().flush();
        check(readUntil(again.getInputStream(), ';', 2000).equals("ID020;"), "after the first one is gone a new connection works (Hamlib reopens)");
        again.close();
        bridge.stop();
        Thread.sleep(100);
        boolean stopped = false;
        try { new Socket("127.0.0.1", port).close(); } catch (IOException e) { stopped = true; }
        check(stopped, "the port is closed when the bridge is stopped");
        System.out.println(failures == 0 ? "all ok" : "FAILED");
        System.exit(failures == 0 ? 0 : 1);
    }
}
