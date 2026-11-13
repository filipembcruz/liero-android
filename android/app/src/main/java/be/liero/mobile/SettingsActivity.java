package be.liero.mobile;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/** PERSONALIZAR / gear: player, CPU difficulty, match rules, touch controls. */
public class SettingsActivity extends Activity {
    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Prefs p = new Prefs(this);
        Ui ui = new Ui(this, "PERSONALIZAR");

        LinearLayout player = ui.card("Seu verme", "Nome exibido na partida");
        EditText name = new EditText(this);
        name.setText(p.name());
        name.setTextColor(Ui.TEXT);
        name.setTypeface(Ui.FONT);
        name.setSingleLine(true);
        name.setFilters(new InputFilter[] { new InputFilter.LengthFilter(20) });
        name.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            public void onTextChanged(CharSequence s, int a, int b, int c) {}
            public void afterTextChanged(Editable s) {
                String v = s.toString().trim();
                p.edit().putString("name", v.isEmpty() ? "Jogador" : v).apply();
            }
        });
        player.addView(name);

        LinearLayout diff = ui.card("Dificuldade da CPU", null);
        TextView info = ui.text(Prefs.DIFFICULTY_INFO[p.difficulty()], 13, Ui.MUTED);
        ui.segmented(diff, Prefs.DIFFICULTY, p.difficulty(), i -> {
            p.edit().putInt("difficulty", i).apply();
            info.setText(Prefs.DIFFICULTY_INFO[i]);
        });
        info.setPadding(0, ui.dp(8), 0, 0);
        diff.addView(info);

        LinearLayout rules = ui.card("Vidas por partida", "Modo Kill'em All do original: perde quem ficar sem vidas");
        String[] lv = new String[Prefs.LIVES.length];
        int sel = 3;
        for (int i = 0; i < lv.length; ++i) { lv[i] = String.valueOf(Prefs.LIVES[i]); if (Prefs.LIVES[i] == p.lives()) sel = i; }
        ui.segmented(rules, lv, sel, i -> p.edit().putInt("lives", Prefs.LIVES[i]).apply());

        LinearLayout touch = ui.card("Controles de toque",
            "Direcional (andar/mirar), Tiro, Pulo, Corda ninja, Trocar arma e Pausa. Controle Bluetooth também funciona.");
        ui.slider(touch, "Tamanho", 60, 160, p.touchScale(), "%", v -> p.edit().putInt("touchScale", v).apply());
        ui.slider(touch, "Opacidade", 15, 100, p.touchOpacity(), "%", v -> p.edit().putInt("touchOpacity", v).apply());
        TextView showTitle = ui.text("Controles na tela", 15, Ui.TEXT);
        showTitle.setPadding(0, ui.dp(8), 0, ui.dp(6));
        touch.addView(showTitle);
        ui.segmented(touch, new String[] { "Automático", "Sempre", "Ocultos" }, p.touchShow(),
            i -> p.edit().putInt("touchShow", i).apply());
        TextView handTitle = ui.text("Lado do direcional", 15, Ui.TEXT);
        handTitle.setPadding(0, ui.dp(8), 0, ui.dp(6));
        touch.addView(handTitle);
        ui.segmented(touch, new String[] { "Esquerda (destro)", "Direita (canhoto)" }, p.lefty() ? 1 : 0,
            i -> p.edit().putBoolean("lefty", i == 1).apply());

        LinearLayout pad = ui.card("Controle Bluetooth",
            "Direcional/analógico: andar e mirar. Os outros botões (atirar, pular, corda, trocar, próxima/anterior arma, "
            + "cavar, pausa) você mapeia na partida: Pausa → Configurações → Controle.");

        LinearLayout game = ui.card("Tempo de carregamento das armas",
            "Recarga das armas (opção LOADING TIMES do original). 100% = original.");
        int[] lt = { 0, 25, 50, 75, 100, 150, 200, 300, 500 };
        String[] ltl = new String[lt.length];
        int ltSel = 4;
        for (int i = 0; i < lt.length; ++i) { ltl[i] = lt[i] + "%"; if (lt[i] == p.loadingTime()) ltSel = i; }
        ui.segmented(game, ltl, ltSel, i -> p.edit().putInt("loadingTime", lt[i]).apply());

        LinearLayout vis = ui.card("Visual", "Campo de visão: Amplo e Máximo mostram mais do mapa e preenchem a tela.");
        ui.segmented(vis, new String[] { "Perto", "Amplo", "Máximo" }, p.fov(), i -> p.edit().putInt("fov", i).apply());
        TextView sm = ui.text("Estilo visual", 15, Ui.TEXT);
        sm.setPadding(0, ui.dp(8), 0, ui.dp(6));
        vis.addView(sm);
        ui.segmented(vis, new String[] { "Clássico", "Suave", "HD" }, p.visual(),
            i -> p.edit().putInt("smooth", i).apply());

        TextView thT = ui.text("Tema do modo HD", 15, Ui.TEXT);
        thT.setPadding(0, ui.dp(8), 0, ui.dp(6));
        vis.addView(thT);
        ui.segmented(vis, new String[] { "Caverna", "Vulcão", "Gelo" }, p.theme(),
            i -> p.edit().putInt("theme", i).apply());

        ui.card("Durante a partida",
            "Toque em Pausa (ou Voltar) para abrir o menu com Configurações: controles, tempo de carregamento, "
            + "vidas, dificuldade, sangue e campo de visão. O menu original do Liero fica em Avançado.");
    }
}
