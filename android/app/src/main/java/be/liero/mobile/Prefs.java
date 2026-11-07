package be.liero.mobile;

import android.content.Context;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Settings shared with the game: mobile.cfg (key=value) in the app's files
 * dir. The game's in-match settings screen writes the same file, so changes
 * made in either place show up in both.
 */
final class Prefs {
    static final String[] DIFFICULTY = { "Fácil", "Médio", "Difícil" };
    static final String[] DIFFICULTY_INFO = {
        "CPU original do Liero 1.33",
        "IA preditiva (olha 0,5 s à frente)",
        "IA preditiva completa (2 s à frente)" };
    static final int[] LIVES = { 3, 5, 10, 15, 20, 30 };

    private final File file;
    private final Map<String, String> map = new LinkedHashMap<>();

    Prefs(Context c) {
        file = new File(c.getFilesDir(), "mobile.cfg");
        if (file.exists()) {
            try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(file), "UTF-8"))) {
                String line;
                while ((line = r.readLine()) != null) {
                    int eq = line.indexOf('=');
                    if (eq > 0) map.put(line.substring(0, eq), line.substring(eq + 1));
                }
            } catch (Exception ignored) {}
        }
    }

    private int i(String k, int def) {
        try { return Integer.parseInt(map.get(k)); } catch (Exception e) { return def; }
    }

    String name() { String n = map.get("name"); return n == null || n.isEmpty() ? "Jogador" : n; }
    int difficulty() { return i("difficulty", 1); }
    int lives() { return i("lives", 15); }
    boolean newMap() { return i("newMap", 1) != 0; }
    int touchScale() { return i("touchScale", 100); }
    int touchOpacity() { return i("touchOpacity", 60); }
    boolean lefty() { return i("lefty", 0) != 0; }
    int loadingTime() { return i("loadingTime", 100); }
    int fov() { return i("fov", 1); }
    int touchShow() { return Math.max(0, Math.min(2, i("touchShow", 0))); }
    int visual() { return Math.max(0, Math.min(2, i("smooth", 2))); }

    final class Editor {
        Editor putString(String k, String v) { map.put(k, v.replace('\n', ' ')); return this; }
        Editor putInt(String k, int v) { map.put(k, String.valueOf(v)); return this; }
        Editor putBoolean(String k, boolean v) { map.put(k, v ? "1" : "0"); return this; }
        void apply() {
            File tmp = new File(file.getPath() + ".tmp");
            try (Writer w = new OutputStreamWriter(new FileOutputStream(tmp), "UTF-8")) {
                for (Map.Entry<String, String> e : map.entrySet()) w.write(e.getKey() + "=" + e.getValue() + "\n");
            } catch (Exception ignored) { return; }
            //noinspection ResultOfMethodCallIgnored
            tmp.renameTo(file);
        }
    }

    Editor edit() { return new Editor(); }
}
