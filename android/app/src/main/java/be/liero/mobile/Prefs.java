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
    static final int[] LIVES = { 1, 3, 5, 10, 15, 20, 30, 50, 99 };
    static final String[] VISUALS = { "Clássico", "Suave", "HD", "Toupeiras", "Soldados", "Robôs" };
    static final String[] VISUAL_INFO = {
        "Pixels originais do Liero", "Pixels suavizados (Scale2x)", "Vermes, armas e cenário desenhados",
        "Toupeiras com sucata numa mina", "Soldados comando com arsenal de brinquedo", "Robôs com armas de energia" };
    static final String[] PAD_ACTIONS = { "Atirar", "Pular", "Corda ninja", "Trocar (segurar)", "Próxima arma", "Arma anterior", "Cavar", "Pausa" };
    static final String[] PAD_KEYS = { "padFire", "padJump", "padRope", "padChange", "padNext", "padPrev", "padDig", "padPause" };
    static final int[] PAD_DEFAULT = { 101, 0, 100, 1, 10, 9, 3, 6 };

    static String padName(int code) {
        switch (code) {
            case 0: return "A"; case 1: return "B"; case 2: return "X"; case 3: return "Y";
            case 4: return "Select/Back"; case 5: return "Guide"; case 6: return "Start";
            case 7: return "L3"; case 8: return "R3"; case 9: return "LB / L1"; case 10: return "RB / R1";
            case 100: return "LT / L2"; case 101: return "RT / R2";
            case -1: return "—";
            default: return "Botão " + code;
        }
    }

    /** Android key code -> SDL game controller button code (as stored in mobile.cfg), or -1. */
    static int padCode(int keyCode) {
        switch (keyCode) {
            case android.view.KeyEvent.KEYCODE_BUTTON_A: return 0;
            case android.view.KeyEvent.KEYCODE_BUTTON_B: return 1;
            case android.view.KeyEvent.KEYCODE_BUTTON_X: return 2;
            case android.view.KeyEvent.KEYCODE_BUTTON_Y: return 3;
            case android.view.KeyEvent.KEYCODE_BUTTON_SELECT: return 4;
            case android.view.KeyEvent.KEYCODE_BUTTON_MODE: return 5;
            case android.view.KeyEvent.KEYCODE_BUTTON_START: return 6;
            case android.view.KeyEvent.KEYCODE_BUTTON_THUMBL: return 7;
            case android.view.KeyEvent.KEYCODE_BUTTON_THUMBR: return 8;
            case android.view.KeyEvent.KEYCODE_BUTTON_L1: return 9;
            case android.view.KeyEvent.KEYCODE_BUTTON_R1: return 10;
            case android.view.KeyEvent.KEYCODE_BUTTON_L2: return 100;
            case android.view.KeyEvent.KEYCODE_BUTTON_R2: return 101;
            default: return -1;
        }
    }

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

    int get(String k, int def) { return i(k, def); }

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
    int theme() { return Math.max(0, Math.min(2, i("theme", 0))); }
    int visual() { return Math.max(0, Math.min(VISUALS.length - 1, i("smooth", 2))); }

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
