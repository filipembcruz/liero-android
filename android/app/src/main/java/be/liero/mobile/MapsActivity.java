package be.liero.mobile;

import java.util.ArrayList;
import java.util.List;

import static be.liero.mobile.PanelView.*;

/** MAPAS: Liero's level generator settings. */
public class MapsActivity extends PanelActivity {
    @Override
    PanelView build() {
        return new PanelView(this, "MAPAS", Icons.MAP, new String[] { "Mapas" }, 0, t -> {
            List<Row> r = new ArrayList<>();
            r.add(stat("Tipo de mapa", "Gerador original do Liero: terra, rochas e materiais", "Aleatório", true));
            r.add(step("Mapa novo a cada partida", "Não = repete o mesmo mapa em Nova partida",
                () -> prefs.newMap() ? "Sim" : "Não", d -> prefs.edit().putInt("newMap", d > 0 ? 1 : 0).apply()));
            r.add(stat("Mapas próprios (.LEV)", "Importar mapas do Liero de PC", "Em breve", false));
            return r;
        }, this::finish);
    }
}
