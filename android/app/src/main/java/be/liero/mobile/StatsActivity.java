package be.liero.mobile;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import static be.liero.mobile.PanelView.*;

/** Stats + trophies, from results.txt written by the game at the end of each match. */
public class StatsActivity extends PanelActivity {
    int[] w = new int[3], l = new int[3], d = new int[3];
    int kills, bestStreak;

    @Override
    PanelView build() {
        load();
        boolean trophies = getIntent().getBooleanExtra("trophies", false);
        return new PanelView(this, trophies ? "CONQUISTAS" : "ESTATÍSTICAS", trophies ? Icons.TROPHY : Icons.BARS,
            new String[] { "Geral", "Por CPU", "Conquistas" }, trophies ? 2 : 0, this::rows, this::finish);
    }

    private void load() {
        int streak = 0;
        File f = new File(getFilesDir(), "results.txt");
        if (!f.exists()) return;
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

    private List<Row> rows(int tab) {
        List<Row> r = new ArrayList<>();
        int tw = w[0] + w[1] + w[2], tl = l[0] + l[1] + l[2], td = d[0] + d[1] + d[2];
        int total = tw + tl + td;
        if (tab == 0) {
            r.add(stat("Partidas", null, String.valueOf(total), true));
            r.add(stat("Vitórias", null, String.valueOf(tw), true));
            r.add(stat("Derrotas", null, String.valueOf(tl), true));
            r.add(stat("Empates", null, String.valueOf(td), true));
            r.add(stat("Aproveitamento", "Vitórias / partidas", total == 0 ? "—" : (100 * tw / total) + "%", true));
            r.add(stat("Abates", "Melhor sequência de vitórias: " + bestStreak, String.valueOf(kills), true));
        } else if (tab == 1) {
            for (int i = 0; i < 3; ++i)
                r.add(stat("CPU " + Prefs.DIFFICULTY[i], Prefs.DIFFICULTY_INFO[i], w[i] + " V · " + l[i] + " D · " + d[i] + " E", w[i] + l[i] + d[i] > 0));
        } else {
            r.add(trophy("Primeiro sangue", "Vença uma partida", tw >= 1));
            r.add(trophy("Veterano", "Jogue 10 partidas", total >= 10));
            r.add(trophy("Exterminador", "Vença 10 partidas", tw >= 10));
            r.add(trophy("Sequência", "Vença 3 seguidas", bestStreak >= 3));
            r.add(trophy("Desafio médio", "Vença a CPU no Médio", w[1] >= 1));
            r.add(trophy("Mestre do Liero", "Vença a CPU no Difícil", w[2] >= 1));
            r.add(trophy("Carnificina", "Some 100 abates", kills >= 100));
        }
        return r;
    }

    private Row trophy(String name, String how, boolean got) {
        return stat(name, how, got ? "Conquistado" : "Bloqueado", got);
    }
}
