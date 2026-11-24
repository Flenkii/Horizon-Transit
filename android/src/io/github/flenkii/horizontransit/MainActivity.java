package io.github.flenkii.horizontransit;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Horizon Transit's phone app: the app's page (assets/index.html, the same page the mod serves) in a WebView, plus
 * what a web page can't do by itself: find the PC in the home network (a UDP call the mod answers), keep the screen
 * on while driving, and buzz. Everything else (the rides, the map, the bus lines) comes from the mod's little server.
 */
public class MainActivity extends Activity {
    static final int FIND_PORT = 8711;
    static final String VERSION = "0.13.0";
    WebView web;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        Window w = getWindow();
        w.setStatusBarColor(Color.rgb(14, 18, 32));
        w.setNavigationBarColor(Color.rgb(23, 28, 44));
        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(14, 18, 32));
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);
        s.setAllowFileAccessFromFileURLs(true);
        s.setAllowUniversalAccessFromFileURLs(true); /* the page (file://) talks to the PC (http://) */
        s.setMediaPlaybackRequiresUserGesture(false);
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest req) {
                Uri u = req.getUrl();
                if ("http".equals(u.getScheme()) && "/api/tile".equals(u.getPath())) return tile(u);
                return null;
            }
        });
        web.setWebChromeClient(new WebChromeClient());
        web.addJavascriptInterface(new Bridge(), "HTApp");
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        setContentView(web);
        if (saved != null) web.restoreState(saved);
        else web.loadUrl("file:///android_asset/index.html");
    }

    @Override
    protected void onSaveInstanceState(Bundle out) {
        super.onSaveInstanceState(out);
        web.saveState(out);
    }

    @Override
    protected void onResume() {
        super.onResume();
        web.onResume();
    }

    @Override
    protected void onPause() {
        web.onPause();
        super.onPause();
    }

    /* 0.10.4: the map pieces stay on the phone (WebP in the app's files), so the map works without the game. Each piece
       comes from the PC while it answers; when it doesn't, from the phone. */
    static volatile long offlineUntil;

    /* 0.13: the pieces of each map version in their own folder ("tiles" = 1, "tiles_v2" ...); a newer version
       throws the old pieces away (the map got roads it didn't have) */
    static volatile int mapVersion = 1;

    File tileDir() {
        int v = mapVersion;
        File d = new File(getFilesDir(), v <= 1 ? "tiles" : "tiles_v" + v);
        if (!d.isDirectory()) {
            File[] old = getFilesDir().listFiles();
            if (old != null)
                for (File o : old) {
                    String n = o.getName();
                    int ov = n.equals("tiles") ? 1 : n.startsWith("tiles_v") ? parseInt(n.substring(7)) : 0;
                    if (ov > 0 && ov < v && o.isDirectory()) {
                        File[] fs = o.listFiles();
                        if (fs != null) for (File f : fs) f.delete();
                        o.delete();
                    }
                }
            d.mkdirs();
        }
        return d;
    }

    static int parseInt(String s) {
        try { return Integer.parseInt(s); } catch (Exception e) { return 0; }
    }

    WebResourceResponse tile(Uri u) {
        String z = u.getQueryParameter("z"), x = u.getQueryParameter("x"), y = u.getQueryParameter("y");
        if (z == null || x == null || y == null || !(z + x + y).matches("[0-9]+")) return null;
        String v = u.getQueryParameter("v");
        if (v != null && v.matches("[0-9]{1,4}") && parseInt(v) > mapVersion) mapVersion = parseInt(v);
        File f = new File(tileDir(), z + "_" + x + "_" + y + ".webp");
        /* 0.11.1: a piece the phone has comes from the phone at once (the map doesn't change); only new ones from the PC */
        if (f.isFile() && f.length() > 0) {
            try {
                return new WebResourceResponse("image/webp", null, new FileInputStream(f));
            } catch (Exception e) {
                /* gone meanwhile: ask the PC */
            }
        }
        if (System.currentTimeMillis() >= offlineUntil) {
            HttpURLConnection c = null;
            boolean reached = false;
            try {
                c = (HttpURLConnection) new URL(u.toString()).openConnection();
                c.setConnectTimeout(1500);
                c.setReadTimeout(8000);
                c.connect();
                reached = true;
                int status = c.getResponseCode();
                if (status == 200) {
                    byte[] data = readAll(c.getInputStream());
                    save(f, data);
                    return new WebResourceResponse("image/bmp", null, new ByteArrayInputStream(data));
                }
                if (status == 403) return new WebResourceResponse("text/plain", "utf-8", 403, "Forbidden", null, new ByteArrayInputStream(new byte[0]));
            } catch (Exception e) {
                /* 0.11.1: only when the PC can't be reached at all; slow or busy (the game runs) is not "offline",
                   the page asks again for that piece */
                if (!reached) offlineUntil = System.currentTimeMillis() + 8000;
            } finally {
                if (c != null) c.disconnect();
            }
        }
        if (f.isFile()) {
            try {
                return new WebResourceResponse("image/webp", null, new FileInputStream(f));
            } catch (Exception e) {
                /* gone meanwhile */
            }
        }
        return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", null, new ByteArrayInputStream(new byte[0]));
    }

    static byte[] readAll(InputStream in) throws java.io.IOException {
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        byte[] buf = new byte[16384];
        int n;
        while ((n = in.read(buf)) > 0) b.write(buf, 0, n);
        in.close();
        return b.toByteArray();
    }

    @SuppressWarnings("deprecation")
    static void save(File f, byte[] bmp) {
        try {
            Bitmap b = BitmapFactory.decodeByteArray(bmp, 0, bmp.length);
            if (b == null) return;
            File tmp = new File(f.getPath() + ".tmp");
            FileOutputStream out = new FileOutputStream(tmp);
            b.compress(Build.VERSION.SDK_INT >= 30 ? Bitmap.CompressFormat.WEBP_LOSSY : Bitmap.CompressFormat.WEBP, 82, out);
            out.close();
            b.recycle();
            if (!tmp.renameTo(f)) tmp.delete();
        } catch (Exception e) {
            /* no room: the map works on without this piece */
        }
    }

    void js(final String code) {
        runOnUiThread(new Runnable() {
            public void run() {
                web.evaluateJavascript(code, null);
            }
        });
    }

    /** what the page can call: window.HTApp.* */
    class Bridge {
        @JavascriptInterface
        public String version() { /* 0.13: from the package itself (versionName and versionCode), shown in the Options */
            try {
                android.content.pm.PackageInfo p = getPackageManager().getPackageInfo(getPackageName(), 0);
                return p.versionName + " (" + p.versionCode + ")";
            } catch (Exception e) {
                return VERSION;
            }
        }

        @JavascriptInterface
        public void vibrate(int ms) {
            Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            if (v == null || ms <= 0) return;
            if (Build.VERSION.SDK_INT >= 26) v.vibrate(VibrationEffect.createOneShot(Math.min(ms, 400), VibrationEffect.DEFAULT_AMPLITUDE));
            else v.vibrate(Math.min(ms, 400));
        }

        @JavascriptInterface
        public void keepAwake(final boolean on) {
            runOnUiThread(new Runnable() {
                public void run() {
                    if (on) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            });
        }

        /** 0.10.1: the bars take the game phone's design (#RRGGBB; light = dark icons) */
        @JavascriptInterface
        public void setColors(final String top, final String bottom, final boolean light) {
            runOnUiThread(new Runnable() {
                public void run() {
                    try {
                        Window w = getWindow();
                        w.setStatusBarColor(Color.parseColor(top));
                        w.setNavigationBarColor(Color.parseColor(bottom));
                        web.setBackgroundColor(Color.parseColor(bottom));
                        int flags = w.getDecorView().getSystemUiVisibility();
                        int dark = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | (Build.VERSION.SDK_INT >= 26 ? View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR : 0);
                        w.getDecorView().setSystemUiVisibility(light ? flags | dark : flags & ~dark);
                    } catch (Exception e) {
                        /* a colour it doesn't know: keep the bars */
                    }
                }
            });
        }

        /** 0.10.4: how many map pieces the phone keeps, and their bytes: "count,bytes" */
        @JavascriptInterface
        public String cacheInfo() {
            File[] all = tileDir().listFiles();
            long bytes = 0;
            int n = 0;
            if (all != null)
                for (File f : all)
                    if (f.getName().endsWith(".webp")) {
                        n++;
                        bytes += f.length();
                    }
            return n + "," + bytes;
        }

        /** calls "HORIZON-TRANSIT?" into the network; the PCs with the mod answer; the page gets htFound([...]) */
        @JavascriptInterface
        public void discover() {
            new Thread(new Runnable() {
                public void run() {
                    js("window.htFound(" + quote(find()) + ")");
                }
            }).start();
        }
    }

    static String quote(String s) {
        StringBuilder b = new StringBuilder("\"");
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c < 0x20) b.append(String.format("\\u%04x", (int) c));
            else b.append(c);
        }
        return b.append('"').toString();
    }

    /** the answers as a JSON list: each the mod's hello with "ip" added */
    static String find() {
        Map<String, String> found = new LinkedHashMap<String, String>();
        DatagramSocket sock = null;
        try {
            sock = new DatagramSocket();
            sock.setBroadcast(true);
            sock.setSoTimeout(400);
            byte[] ask = "HORIZON-TRANSIT?".getBytes("UTF-8");
            List<InetAddress> targets = new ArrayList<InetAddress>();
            targets.add(InetAddress.getByName("255.255.255.255"));
            Enumeration<NetworkInterface> all = NetworkInterface.getNetworkInterfaces();
            while (all != null && all.hasMoreElements()) {
                NetworkInterface ni = all.nextElement();
                if (!ni.isUp() || ni.isLoopback()) continue;
                for (InterfaceAddress a : ni.getInterfaceAddresses())
                    if (a.getBroadcast() != null) targets.add(a.getBroadcast());
            }
            long end = System.currentTimeMillis() + 2000;
            for (int round = 0; round < 3 && System.currentTimeMillis() < end; round++) {
                for (InetAddress t : targets) {
                    try {
                        sock.send(new DatagramPacket(ask, ask.length, t, FIND_PORT));
                    } catch (Exception e) {
                        /* that network doesn't take broadcasts: the others may */
                    }
                }
                long roundEnd = Math.min(end, System.currentTimeMillis() + 650);
                while (System.currentTimeMillis() < roundEnd) {
                    byte[] buf = new byte[512];
                    DatagramPacket p = new DatagramPacket(buf, buf.length);
                    try {
                        sock.receive(p);
                    } catch (SocketTimeoutException e) {
                        break;
                    }
                    String text = new String(p.getData(), 0, p.getLength(), "UTF-8").trim();
                    String ip = p.getAddress().getHostAddress();
                    if (text.startsWith("{") && text.endsWith("}") && text.contains("HorizonTransit") && !found.containsKey(ip))
                        found.put(ip, text.substring(0, text.length() - 1) + ",\"ip\":\"" + ip + "\"}");
                }
            }
        } catch (Exception e) {
            /* no network: nothing found */
        } finally {
            if (sock != null) sock.close();
        }
        StringBuilder b = new StringBuilder("[");
        for (String v : found.values()) {
            if (b.length() > 1) b.append(',');
            b.append(v);
        }
        return b.append(']').toString();
    }
}
