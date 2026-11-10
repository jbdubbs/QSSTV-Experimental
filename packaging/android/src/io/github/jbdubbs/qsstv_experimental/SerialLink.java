package io.github.jbdubbs.qsstv_experimental;

import java.io.IOException;

/**
 * A serial line to a radio or an interface, as far as the CAT bridge and the PTT keying need it. UsbSerialHelper has one that
 * is a USB serial adapter; tests/catbridge has one that is a radio made of a few lines of Java.
 */
public interface SerialLink
{
    /** Bytes that arrived, up to buffer.length, waiting at most timeoutMs; 0 when nothing came */
    int read(byte[] buffer, int timeoutMs) throws IOException;

    /** Sends the first length bytes of data */
    void write(byte[] data, int length, int timeoutMs) throws IOException;

    void setRts(boolean on) throws IOException;

    void setDtr(boolean on) throws IOException;

    void close();
}
