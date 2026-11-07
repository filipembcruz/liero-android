package be.liero.mobile;

import android.app.Activity;
import android.os.Bundle;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

/** Stats + trophies, from results.txt written by the game at the end of each match. */
public class StatsActivity extends Activity {
    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        boolean trophies = getIntent().getBooleanExtra("trophies", false);
        Ui ui = new Ui(this, trophies ? "CONQUISTAS" : "ESTATÍSTICAS");

        int[] w = new int[3], l = new int[3], d = new int[3];
        int kills = 0, streak = 0, bestStreak = 0;
        File f = new File(getFilesDir(), "results.txt");
        if (f.exists()) {
            try (BufferedReader r = new BufferedReader(new FileReader(f))) {
                String line;
                while ((line = r.readLine()) != null) {
                    String[] s = line.trim().split("\\s+");
                    if (s.length < 4) continue;
                    int diff = Math.max(0, Math.min(2, Integer.parseInt(s[3])));
                    kills += Integer.parseInt(s[1]);
                    switch (s[0]) {
                        case "W": w[diff]++; streak++; bestStreak = Math.max(bestStreak, streak); break;
                        case "L": l[diff]++; streak = 0; break;
                        default: d[diff]++; break;
                    }
                }
            } catch (Exception ignored) {}
        }
        int tw = w[0] + w[1] + w[2], tl = l[0] + l[1] + l[2], td = d[0] + d[1] + d[2];
        int total = tw + tl + td;

        if (!trophies) {
            LinearLayout all = ui.card("Geral", null);
            all.addView(ui.text("Partidas: " + total, 16, Ui.TEXT));
            all.addView(ui.text("Vitórias: " + tw + "   Derrotas: " + tl + "   Empates: " + td, 16, Ui.TEXT));
            all.addView(ui.text("Aproveitamento: " + (total == 0 ? "-" : (100 * tw / total) + "%"), 16, Ui.YELLOW));
            all.addView(ui.text("Abates: " + kills + "   Melhor sequência: " + bestStreak, 16, Ui.TEXT));
            for (int i = 0; i < 3; ++i) {
                LinearLayout c = ui.card("CPU " + Prefs.DIFFICULTY[i], null);
                c.addView(ui.text(w[i] + " V  ·  " + l[i] + " D  ·  " + d[i] + " E", 16, Ui.TEXT));
            }
        } else {
            trophy(ui, "Primeiro sangue", "Vença uma partida", tw >= 1);
            trophy(ui, "Veterano", "Jogue 10 partidas", total >= 10);
            trophy(ui, "Exterminador", "Vença 10 partidas", tw >= 10);
            trophy(ui, "Sequência", "Vença 3 seguidas", bestStreak >= 3);
            trophy(ui, "Desafio médio", "Vença a CPU no Médio", w[1] >= 1);
            trophy(ui, "Mestre do Liero", "Vença a CPU no Difícil", w[2] >= 1);
            trophy(ui, "Carnificina", "Some 100 abates", kills >= 100);
        }
    }

    private void trophy(Ui ui, String name, String how, boolean got) {
        LinearLayout c = ui.card((got ? "🏆 " : "🔒 ") + name, how);
        TextView t = ui.text(got ? "CONQUISTADO" : "bloqueado", 14, got ? Ui.YELLOW : Ui.MUTED);
        c.addView(t);
    }
}
