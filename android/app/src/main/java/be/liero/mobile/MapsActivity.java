package be.liero.mobile;

import android.app.Activity;
import android.os.Bundle;
import android.widget.LinearLayout;

/** MAPAS: Liero's level generator settings. */
public class MapsActivity extends Activity {
    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Prefs p = new Prefs(this);
        Ui ui = new Ui(this, "MAPAS");
        LinearLayout gen = ui.card("Mapa aleatório do Liero",
            "Cavernas geradas pelo gerador original (terra, rochas e materiais do Liero 1.33). "
            + "Cada vez que você toca em JOGAR, um mapa novo é criado.");
        LinearLayout regen = ui.card("Ao escolher NEW GAME no menu do jogo", null);
        ui.segmented(regen, new String[] { "Gerar mapa novo", "Repetir o mapa" }, p.newMap() ? 0 : 1,
            i -> p.edit().putBoolean("newMap", i == 0).apply());
        ui.card("Mapas próprios (.LEV)",
            "Suporte a importar mapas .LEV do Liero ainda não está nesta versão.");
    }
}
