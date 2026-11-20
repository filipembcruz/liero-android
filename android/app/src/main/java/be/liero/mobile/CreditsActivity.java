package be.liero.mobile;

import java.util.ArrayList;
import java.util.List;

import static be.liero.mobile.PanelView.*;

/** Credits and third-party licenses (required by the licenses of the bundled code/data). */
public class CreditsActivity extends PanelActivity {
    @Override
    PanelView build() {
        return new PanelView(this, "CRÉDITOS", Icons.HEART, new String[] { "Créditos" }, 0, t -> {
            List<Row> r = new ArrayList<>();
            r.add(stat("Tunnel Clash", "Lirobyte", "v" + version(), true));
            r.add(note("Baseado no Liero (Joosa Riekkinen, 1998) e no OpenLiero 1.36 (Erik Lindroos e comunidade)."));
            r.add(note("Dados originais do Liero e sons do lierolibre: licença WTFPL."));
            r.add(note("SDL 2 (zlib) · fonte Poppins (SIL OFL 1.1) · miniz, stb_vorbis (MIT / domínio público)."));
            r.add(note("Personagens, armas, temas e telas criados para o Tunnel Clash."));
            r.add(note("Textos completos das licenças: github.com/filipembcruz/liero-android"));
            return r;
        }, this::finish);
    }

    String version() {
        try { return getPackageManager().getPackageInfo(getPackageName(), 0).versionName; }
        catch (Exception e) { return "?"; }
    }
}
