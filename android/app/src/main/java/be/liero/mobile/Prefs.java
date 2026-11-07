package be.liero.mobile;

import android.content.Context;
import android.content.SharedPreferences;

/** Launcher settings, persisted in SharedPreferences (survive app close). */
final class Prefs {
    static final String FILE = "liero";
    static final String[] DIFFICULTY = { "Fácil", "Médio", "Difícil" };
    static final String[] DIFFICULTY_INFO = {
        "CPU original do Liero 1.33",
        "IA preditiva (olha 0,5 s à frente)",
        "IA preditiva completa (2 s à frente)" };
    static final int[] LIVES = { 3, 5, 10, 15, 20, 30 };

    final SharedPreferences sp;

    Prefs(Context c) { sp = c.getSharedPreferences(FILE, Context.MODE_PRIVATE); }

    String name() { return sp.getString("name", "Jogador"); }
    int difficulty() { return sp.getInt("difficulty", 1); }
    int lives() { return sp.getInt("lives", 15); }
    boolean newMap() { return sp.getBoolean("newMap", true); }
    int touchScale() { return sp.getInt("touchScale", 100); }   // percent
    int touchOpacity() { return sp.getInt("touchOpacity", 55); } // percent
    boolean lefty() { return sp.getBoolean("lefty", false); }
    boolean vibrate() { return sp.getBoolean("vibrate", true); }

    SharedPreferences.Editor edit() { return sp.edit(); }
}
