package io.github.jbdubbs.qsstv_experimental;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.ConnectException;
import java.net.HttpURLConnection;
import java.net.NoRouteToHostException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.net.UnknownHostException;

/**
 * One HTTPS GET through the system's own HttpsURLConnection (the system's certificates, no OpenSSL of the program's own: Qt's
 * Android build has no TLS library to load). Used by the city and state lookup (src/gps/placelookup.cpp through
 * src/android/androidhttp.cpp). The request runs on a thread of its own; the answer is handed to done() once.
 */
public class HttpHelper
{
    /** the answer, for the token get() was given: status 0 and a reason in error when there was no HTTP answer at all (a timeout is "timeout") */
    private static native void done(long token, int status, String body, String error);

    private static final int MAX_BODY = 256 * 1024;   // a place is a few hundred bytes; nothing bigger is read

    public static void get(final long token, final String url, final String userAgent, final int timeoutMs)
    {
        new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                HttpURLConnection c = null;
                try
                {
                    c = (HttpURLConnection) new URL(url).openConnection();
                    c.setConnectTimeout(timeoutMs);
                    c.setReadTimeout(timeoutMs);
                    c.setRequestMethod("GET");
                    c.setRequestProperty("User-Agent", userAgent);
                    c.setRequestProperty("Accept", "application/json");
                    c.setInstanceFollowRedirects(true);   // the same scheme only: HttpURLConnection never follows https to http
                    final int status = c.getResponseCode();
                    InputStream in = status >= 400 ? c.getErrorStream() : c.getInputStream();
                    String body = "";
                    if (in != null)
                    {
                        ByteArrayOutputStream out = new ByteArrayOutputStream();
                        byte[] buf = new byte[4096];
                        int n;
                        while ((n = in.read(buf)) > 0 && out.size() < MAX_BODY) out.write(buf, 0, n);
                        in.close();
                        body = out.toString("UTF-8");
                    }
                    done(token, status, body, "");
                }
                catch (SocketTimeoutException e)
                {
                    done(token, 0, "", "timeout");
                }
                catch (UnknownHostException e)
                {
                    done(token, 0, "", "no internet connection");   // no name resolution: offline, airplane mode, no data
                }
                catch (ConnectException e)
                {
                    done(token, 0, "", "no internet connection");
                }
                catch (NoRouteToHostException e)
                {
                    done(token, 0, "", "no internet connection");
                }
                catch (Throwable e)
                {
                    done(token, 0, "", e.getClass().getSimpleName() + ": " + e.getMessage());
                }
                finally
                {
                    if (c != null) c.disconnect();
                }
            }
        }, "qsstv-http").start();
    }
}
