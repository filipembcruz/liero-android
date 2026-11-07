package be.liero.mobile;

import android.content.Context;
import android.content.res.AssetManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Copies the game data (TC/openliero: rules, sprites, sounds) from the APK
 * assets to internal storage, where the original C++ code reads it with plain
 * file I/O. Only re-copies when the data version changes; liero.cfg (the
 * game's own settings) is never touched.
 */
final class AssetInstaller {
    interface Progress { void onProgress(float p); }

    static File dataDir(Context c) { return c.getFilesDir(); }

    static void install(Context c, Progress cb) throws Exception {
        AssetManager am = c.getAssets();
        List<String> files = new ArrayList<>();
        String version;
        try (BufferedReader r = new BufferedReader(new InputStreamReader(am.open("liero_files.txt"), "UTF-8"))) {
            version = r.readLine();
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (!line.isEmpty()) files.add(line);
            }
        }
        File stamp = new File(dataDir(c), ".data_version");
        if (stamp.exists() && version.equals(readAll(stamp)) && new File(dataDir(c), "TC/openliero/tc.cfg").exists()) {
            cb.onProgress(1f);
            return;
        }
        byte[] buf = new byte[64 * 1024];
        for (int i = 0; i < files.size(); ++i) {
            String rel = files.get(i);
            File out = new File(dataDir(c), rel);
            File parent = out.getParentFile();
            if (parent != null) parent.mkdirs();
            try (InputStream in = am.open(rel); OutputStream os = new FileOutputStream(out)) {
                int n;
                while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
            }
            cb.onProgress((i + 1) / (float) files.size());
        }
        try (OutputStream os = new FileOutputStream(stamp)) { os.write(version.getBytes("UTF-8")); }
    }

    private static String readAll(File f) {
        try (BufferedReader r = new BufferedReader(new InputStreamReader(new java.io.FileInputStream(f), "UTF-8"))) {
            String l = r.readLine();
            return l == null ? "" : l.trim();
        } catch (Exception e) {
            return "";
        }
    }
}
