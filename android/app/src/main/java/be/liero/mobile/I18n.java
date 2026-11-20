package be.liero.mobile;

import android.content.Context;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Locale;

/** UI translations shared with the native game (assets ui/lang.tsv, keys = Portuguese text). */
final class I18n {
    static final String[] CODES = { "pt", "en", "es", "fr", "de" };
    static final String[] NAMES = { "Português", "English", "Español", "Français", "Deutsch" };

    private static HashMap<String, String[]> table;
    private static int column = 1;

    static void init(Context c) {
        if (table == null) {
            table = new HashMap<>();
            try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getAssets().open("ui/lang.tsv"), StandardCharsets.UTF_8))) {
                String line;
                boolean header = true;
                while ((line = r.readLine()) != null) {
                    if (header) { header = false; continue; }
                    if (line.isEmpty() || line.startsWith("#")) continue;
                    String[] cols = line.split("\t", -1);
                    for (int i = 0; i < cols.length; ++i) cols[i] = cols[i].replace("\\n", "\n");
                    if (cols.length > 0 && !cols[0].isEmpty()) table.put(cols[0], cols);
                }
            } catch (Exception ignored) {}
        }
        setLanguage(new Prefs(c).get("lang", 0));
    }

    /** lang: 0 = device language, 1.. = CODES[lang - 1]. */
    static void setLanguage(int lang) {
        if (lang >= 1 && lang <= CODES.length) { column = lang - 1; return; }
        String sys = Locale.getDefault().getLanguage();
        column = 1;
        for (int i = 0; i < CODES.length; ++i) if (CODES[i].equals(sys)) column = i;
    }

    static int current() { return column; }

    static String t(String pt) {
        if (pt == null || column == 0 || table == null) return pt;
        String[] row = table.get(pt);
        if (row == null || row.length <= column || row[column].isEmpty()) return pt;
        return row[column];
    }
}
