package be.liero.mobile;

import android.app.AlertDialog;
import android.text.InputFilter;
import android.widget.EditText;

import java.util.ArrayList;
import java.util.List;

import static be.liero.mobile.PanelView.*;

/** PERSONALIZAR / gear: same compact panel and options as the in-game settings. */
public class SettingsActivity extends PanelActivity {

    private void put(String k, int v) { prefs.edit().putInt(k, v).apply(); }
    private int get(String k, int def) { return prefs.get(k, def); }
    private int clamp(int v, int a, int b) { return Math.max(a, Math.min(b, v)); }

    @Override
    PanelView build() {
        String[] tabs = { "Partida", "Toque", "Controle", "Visual" };
        int start = getIntent().getIntExtra("tab", 0);
        return new PanelView(this, "PERSONALIZAR", Icons.GEAR, tabs, start, this::rows, this::finish) {
            @Override void onOverlayCancelled() { capturing = -1; }
        };
    }

    private List<Row> rows(int tab) {
        List<Row> r = new ArrayList<>();
        switch (tab) {
            case 0: {
                r.add(edit("Seu nome", "Toque para mudar", prefs::name, d -> editName()));
                r.add(step("Dificuldade da CPU", Prefs.DIFFICULTY_INFO[prefs.difficulty()],
                    () -> Prefs.DIFFICULTY[prefs.difficulty()], d -> put("difficulty", clamp(prefs.difficulty() + d, 0, 2))));
                r.add(step("Vidas", "Modo Kill'em All: perde quem ficar sem vidas",
                    () -> String.valueOf(prefs.lives()), d -> put("lives", cycle(prefs.lives(), d, Prefs.LIVES))));
                int[] lt = { 0, 25, 50, 75, 100, 150, 200, 300, 500 };
                r.add(step("Tempo de carregamento", "Recarga das armas (original: LOADING TIMES)",
                    () -> pct(prefs.loadingTime()), d -> put("loadingTime", cycle(prefs.loadingTime(), d, lt))));
                int[] bl = { 0, 50, 100, 200, 300, 500 };
                r.add(step("Quantidade de sangue", null, () -> pct(get("blood", 100)), d -> put("blood", cycle(get("blood", 100), d, bl))));
                r.add(step("Mapa novo a cada partida", null, () -> prefs.newMap() ? "Sim" : "Não", d -> put("newMap", d > 0 ? 1 : 0)));
                break;
            }
            case 1: {
                r.add(step("Tamanho dos botões", null, () -> pct(prefs.touchScale()), d -> put("touchScale", clamp(prefs.touchScale() + d * 10, 50, 180))));
                r.add(step("Opacidade", null, () -> pct(prefs.touchOpacity()), d -> put("touchOpacity", clamp(prefs.touchOpacity() + d * 10, 10, 100))));
                r.add(step("Zona morta do direcional", "Menor = responde com toque mais curto",
                    () -> pct(get("deadZone", 22)), d -> put("deadZone", clamp(get("deadZone", 22) + d * 5, 5, 60))));
                r.add(step("Lado do direcional", null, () -> prefs.lefty() ? "Direita" : "Esquerda", d -> put("lefty", d > 0 ? 1 : 0)));
                r.add(step("Botão Cavar", "Cava túneis (= esquerda + direita juntos)",
                    () -> get("digButton", 1) != 0 ? "Ligado" : "Desligado", d -> put("digButton", d > 0 ? 1 : 0)));
                String[] ts = { "Automático", "Sempre", "Ocultos" };
                r.add(step("Controles na tela", "Automático: somem quando você usa um controle externo",
                    () -> ts[prefs.touchShow()], d -> put("touchShow", clamp(prefs.touchShow() + d, 0, 2))));
                break;
            }
            case 2: {
                r.add(note("Toque numa função e aperte o botão do controle. O direcional (D-pad/analógico) é fixo."));
                for (int a = 0; a < Prefs.PAD_ACTIONS.length; ++a) {
                    final int act = a;
                    r.add(pad(Prefs.PAD_ACTIONS[a], () -> Prefs.padName(get(Prefs.PAD_KEYS[act], Prefs.PAD_DEFAULT[act])), d -> {
                        capturing = act;
                        panel.overlay = "Aperte o botão para: " + Prefs.PAD_ACTIONS[act];
                        panel.invalidate();
                    }));
                }
                r.add(step("Vibração ao tocar", null, () -> get("vibrate", 1) != 0 ? "Ligada" : "Desligada", d -> put("vibrate", d > 0 ? 1 : 0)));
                r.add(button("PADRÃO", Icons.RESTART, d -> {
                    Prefs.Editor e = prefs.edit();
                    for (int a = 0; a < Prefs.PAD_KEYS.length; ++a) e.putInt(Prefs.PAD_KEYS[a], Prefs.PAD_DEFAULT[a]);
                    e.apply();
                }));
                break;
            }
            default: {
                String[] fv = { "Perto", "Amplo", "Máximo" };
                r.add(step("Campo de visão", "Amplo/Máximo mostram mais do mapa", () -> fv[prefs.fov()], d -> put("fov", clamp(prefs.fov() + d, 0, 2))));
                r.add(step("Visual", Prefs.VISUAL_INFO[prefs.visual()], () -> Prefs.VISUALS[prefs.visual()],
                    d -> put("smooth", clamp(prefs.visual() + d, 0, Prefs.VISUALS.length - 1))));
                r.add(step("Minimapa", null, () -> get("minimap", 1) != 0 ? "Ligado" : "Desligado", d -> put("minimap", d > 0 ? 1 : 0)));
                r.add(step("Opacidade do HUD", "Placar, minimapa e barra de armas",
                    () -> pct(get("hudOpacity", 100)), d -> put("hudOpacity", clamp(get("hudOpacity", 100) + d * 10, 20, 100))));
                r.add(step("HUD transparente sobre você", "O painel some quando o seu personagem passa por baixo",
                    () -> get("hudAutoFade", 1) != 0 ? "Ligado" : "Desligado", d -> put("hudAutoFade", d > 0 ? 1 : 0)));
                r.add(step("Seta sobre o seu personagem", null, () -> get("marker", 1) != 0 ? "Ligada" : "Desligada", d -> put("marker", d > 0 ? 1 : 0)));
                break;
            }
        }
        return r;
    }

    @Override
    void onCaptured(int action, int code) {
        Prefs.Editor e = prefs.edit();
        int old = get(Prefs.PAD_KEYS[action], Prefs.PAD_DEFAULT[action]);
        for (int b = 0; b < Prefs.PAD_KEYS.length; ++b)
            if (b != action && get(Prefs.PAD_KEYS[b], Prefs.PAD_DEFAULT[b]) == code) e.putInt(Prefs.PAD_KEYS[b], old);
        e.putInt(Prefs.PAD_KEYS[action], code);
        e.apply();
    }

    private void editName() {
        EditText t = new EditText(this);
        t.setText(prefs.name());
        t.setSingleLine(true);
        t.setFilters(new InputFilter[] { new InputFilter.LengthFilter(20) });
        t.setSelectAllOnFocus(true);
        new AlertDialog.Builder(this)
            .setTitle("Seu nome")
            .setView(t)
            .setPositiveButton("OK", (dlg, w) -> {
                String v = t.getText().toString().trim();
                prefs.edit().putString("name", v.isEmpty() ? "Jogador" : v).apply();
                panel.invalidate();
                hideSystemUi();
            })
            .setNegativeButton("Cancelar", (dlg, w) -> hideSystemUi())
            .show();
    }
}
