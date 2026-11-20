package be.liero.mobile;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Start screen. The scenery and the logo are separate images and the buttons
 * are drawn by the app, so the screen fills any phone aspect ratio without
 * bars. Multiplayer is shown but disabled (single player only for now).
 */
public class LauncherActivity extends Activity {

    private MenuView menu;
    private LoadingView loading;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private boolean starting;
    static Typeface font;

    static final String[] TIPS = {
        "Explore o mapa em busca de armas, munição e kits de vida.",
        "Segure Trocar e use o direcional para escolher a arma.",
        "A corda ninja prende em qualquer parede: balance e ataque de cima!",
        "O botão Cavar abre túneis sem precisar parar de mirar.",
        "Toque numa arma da barra de baixo para trocar na hora.",
        "Explosões cavam o mapa: use isso para emboscar a CPU.",
    };

    static Typeface font(Context c) {
        if (font == null) {
            try { font = Typeface.createFromAsset(c.getAssets(), "fonts/Poppins-Bold.ttf"); }
            catch (Exception e) { font = Typeface.DEFAULT_BOLD; }
        }
        return font;
    }

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        I18n.init(this);
        font(this);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        menu = new MenuView(this);
        setContentView(menu);
        hideSystemUi();
        new Thread(() -> {
            try { AssetInstaller.install(this, p -> {}); } catch (Exception ignored) {}
        }).start();
        if (getIntent() != null && getIntent().getBooleanExtra("screenshot_loading", false)) {
            showLoading(0.62f);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        starting = false;
        if (loading != null && !getIntent().getBooleanExtra("screenshot_loading", false)) {
            setContentView(menu);
            loading = null;
        }
        hideSystemUi();
        if (backFromGame) {
            backFromGame = false;
            maybeAskSupport();
        }
    }

    private boolean backFromGame;

    /** After a few matches, invite the player (gently, now and then) to support the developer. */
    private void maybeAskSupport() {
        int matches = 0;
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(new java.io.File(getFilesDir(), "results.txt")))) {
            while (r.readLine() != null) matches++;
        } catch (Exception ignored) {}
        Prefs p = new Prefs(this);
        int last = p.get("supportAskedAt", 0);
        int every = p.get("supporter", 0) != 0 ? 25 : 6; // supporters are asked much less often
        if (matches >= 3 && matches - last >= (last == 0 ? 3 : every)) {
            p.edit().putInt("supportAskedAt", matches).apply();
            startActivity(new Intent(this, SupportActivity.class).putExtra("prompt", true));
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    private void hideSystemUi() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    private void showLoading(float fixed) {
        loading = new LoadingView(this);
        loading.progress = fixed;
        setContentView(loading);
    }

    void play() {
        if (starting) return;
        starting = true;
        backFromGame = true;
        showLoading(0f);
        final long t0 = SystemClock.uptimeMillis();
        new Thread(() -> {
            try {
                AssetInstaller.install(this, p -> ui.post(() -> loading.setProgress(p * 0.85f)));
            } catch (Exception e) {
                ui.post(() -> {
                    Toast.makeText(this, I18n.t("Erro ao instalar dados: ") + e.getMessage(), Toast.LENGTH_LONG).show();
                    setContentView(menu);
                    starting = false;
                });
                return;
            }
            long minMs = 700;
            while (SystemClock.uptimeMillis() - t0 < minMs) {
                float f = (SystemClock.uptimeMillis() - t0) / (float) minMs;
                ui.post(() -> loading.setProgress(Math.max(loading.progress, 0.85f + 0.15f * f)));
                SystemClock.sleep(16);
            }
            ui.post(() -> {
                loading.setProgress(1f);
                Intent i = new Intent(this, GameActivity.class);
                i.putExtra("autostart", true);
                startActivity(i);
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            });
        }).start();
    }

    // ------------------------------------------------------------------

    /** Scenery (cover-cropped to any aspect) + logo. */
    abstract static class SceneView extends View {
        final Bitmap scene, logo;
        final Matrix cover = new Matrix();
        final Paint bmp = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        final Paint vignette = new Paint();
        float u = 1; // 1 unit = 1/720 of the screen height
        float pan = 0;
        final long t0 = SystemClock.uptimeMillis();

        SceneView(Context c) {
            super(c);
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inScaled = false;
            scene = BitmapFactory.decodeResource(c.getResources(), R.drawable.menu_scene, o);
            logo = BitmapFactory.decodeResource(c.getResources(), R.drawable.logo, o);
        }

        @Override
        protected void onSizeChanged(int w, int h, int ow, int oh) {
            u = h / 720f;
            vignette.setShader(new LinearGradient(0, 0, 0, h,
                new int[] { Color.argb(90, 10, 6, 30), Color.argb(0, 0, 0, 0), Color.argb(150, 10, 6, 30) },
                new float[] { 0f, 0.45f, 1f }, Shader.TileMode.CLAMP));
        }

        void drawScene(Canvas c, boolean animate) {
            int w = getWidth(), h = getHeight();
            float sw = scene.getWidth(), sh = scene.getHeight();
            float k = Math.max(w / sw, h / sh) * 1.06f; // a little extra room for a slow pan
            float extra = sw * k - w;
            if (animate) pan = (float) Math.sin((SystemClock.uptimeMillis() - t0) / 9000.0) * 0.5f + 0.5f;
            cover.reset();
            cover.postScale(k, k);
            cover.postTranslate(-extra * pan, (h - sh * k) / 2f);
            c.drawBitmap(scene, cover, bmp);
            c.drawRect(0, 0, w, h, vignette);
        }

        RectF drawLogo(Canvas c, float cx, float top, float maxW, float maxH) {
            float k = Math.min(maxW / logo.getWidth(), maxH / logo.getHeight());
            RectF r = new RectF(cx - logo.getWidth() * k / 2, top, cx + logo.getWidth() * k / 2, top + logo.getHeight() * k);
            c.drawBitmap(logo, null, r, bmp);
            return r;
        }
    }

    static final int Z_PLAY = 1, Z_MULTI = 2, Z_CUSTOM = 3, Z_MAPS = 4, Z_GEAR = 5, Z_STATS = 6, Z_TROPHY = 7;

    static final class Btn {
        RectF r = new RectF();
        int id; String label; int icon; boolean primary, disabled, square, heart;
        Btn(int id, String label, int icon, boolean primary, boolean disabled, boolean square) {
            this.id = id; this.label = label; this.icon = icon; this.primary = primary; this.disabled = disabled; this.square = square;
        }
    }

    final class MenuView extends SceneView {
        final List<Btn> btns = new ArrayList<>();
        Btn pressed;
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final RectF credits = new RectF();
        boolean creditsDown;

        MenuView(Context c) {
            super(c);
            btns.add(new Btn(Z_PLAY, "JOGAR", Icons.PLAY, true, false, false));
            Btn sup = new Btn(Z_MULTI, "APOIE O DEV", Icons.HEART, false, false, false);
            sup.heart = true;
            btns.add(sup);
            btns.add(new Btn(Z_CUSTOM, "PERSONALIZAR", Icons.TOOLS, false, false, false));
            btns.add(new Btn(Z_MAPS, "MAPAS", Icons.MAP, false, false, false));
            btns.add(new Btn(Z_GEAR, null, Icons.GEAR, false, false, true));
            btns.add(new Btn(Z_STATS, null, Icons.BARS, false, false, true));
            btns.add(new Btn(Z_TROPHY, null, Icons.TROPHY, false, false, true));
        }

        void layoutButtons() {
            int w = getWidth(), h = getHeight();
            float cx = w / 2f;
            float bw = Math.min(w * 0.36f, 520 * u), playH = 92 * u, bh = 70 * u, gap = 14 * u;
            float y = h * 0.47f;
            float sq = 72 * u, m = 24 * u;
            for (Btn b : btns) {
                switch (b.id) {
                    case Z_PLAY: b.r.set(cx - bw / 2, y, cx + bw / 2, y + playH); y += playH + gap * 1.6f; break;
                    case Z_MULTI: case Z_CUSTOM: case Z_MAPS:
                        float sw = bw * 0.9f;
                        b.r.set(cx - sw / 2, y, cx + sw / 2, y + bh); y += bh + gap; break;
                    case Z_GEAR: b.r.set(m, 18 * u, m + sq, 18 * u + sq); break;
                    case Z_STATS: b.r.set(w - m - sq * 2 - 14 * u, 18 * u, w - m - sq - 14 * u, 18 * u + sq); break;
                    case Z_TROPHY: b.r.set(w - m - sq, 18 * u, w - m, 18 * u + sq); break;
                }
            }
        }

        @Override
        protected void onDraw(Canvas c) {
            drawScene(c, true);
            layoutButtons();
            drawLogo(c, getWidth() / 2f, 10 * u, Math.min(getWidth() * 0.5f, 640 * u), getHeight() * 0.40f);
            for (Btn b : btns) Icons.button(c, p, b, b == pressed, u, font);
            // discreet credits link (bottom-left)
            p.setShader(null);
            p.setTypeface(font);
            p.setTextSize(17 * u);
            p.setTextAlign(Paint.Align.LEFT);
            p.setColor(Color.argb(creditsDown ? 255 : 170, 220, 225, 255));
            String cl = I18n.t("Créditos e licenças");
            c.drawText(cl, 24 * u, getHeight() - 22 * u, p);
            credits.set(10 * u, getHeight() - 60 * u, 34 * u + p.measureText(cl), getHeight());
            postInvalidateOnAnimation();
        }

        Btn hit(float x, float y) {
            for (Btn b : btns) {
                RectF r = new RectF(b.r);
                r.inset(-8 * u, -8 * u);
                if (r.contains(x, y)) return b;
            }
            return null;
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    creditsDown = credits.contains(e.getX(), e.getY());
                    pressed = hit(e.getX(), e.getY());
                    if (pressed != null) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if (pressed != null && hit(e.getX(), e.getY()) != pressed) pressed = null;
                    return true;
                case MotionEvent.ACTION_UP:
                    if (creditsDown && credits.contains(e.getX(), e.getY()))
                        getContext().startActivity(new Intent(getContext(), CreditsActivity.class));
                    creditsDown = false;
                    Btn b = pressed;
                    pressed = null;
                    if (b != null && hit(e.getX(), e.getY()) == b) onButton(b);
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    pressed = null;
                    return true;
            }
            return super.onTouchEvent(e);
        }
    }

    void onButton(Btn b) {
        switch (b.id) {
            case Z_PLAY: play(); break;
            case Z_MULTI: startActivity(new Intent(this, SupportActivity.class)); break;
            case Z_CUSTOM: case Z_GEAR: startActivity(new Intent(this, SettingsActivity.class)); break;
            case Z_MAPS: startActivity(new Intent(this, MapsActivity.class)); break;
            case Z_STATS: startActivity(new Intent(this, StatsActivity.class)); break;
            case Z_TROPHY: startActivity(new Intent(this, StatsActivity.class).putExtra("trophies", true)); break;
        }
    }

    /** Loading screen: scenery + logo + live progress bar + tip. */
    static final class LoadingView extends SceneView {
        float progress;
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final String tip = TIPS[(int) (System.currentTimeMillis() / 1000 % TIPS.length)];

        LoadingView(Context c) { super(c); }

        void setProgress(float v) { progress = Math.max(0, Math.min(1, v)); invalidate(); }

        @Override
        protected void onDraw(Canvas c) {
            drawScene(c, true);
            int w = getWidth(), h = getHeight();
            drawLogo(c, w / 2f, 20 * u, Math.min(w * 0.55f, 700 * u), h * 0.46f);
            p.setTypeface(font);
            p.setTextAlign(Paint.Align.CENTER);
            p.setTextSize(30 * u);
            p.setColor(Color.WHITE);
            p.setShadowLayer(4 * u, 0, 2 * u, Color.argb(160, 0, 0, 0));
            c.drawText(I18n.t("CARREGANDO…"), w / 2f, h * 0.70f, p);
            p.clearShadowLayer();
            float bw = Math.min(w * 0.55f, 900 * u), bh = 30 * u;
            RectF bar = new RectF(w / 2f - bw / 2, h * 0.74f, w / 2f + bw / 2, h * 0.74f + bh);
            p.setShader(null);
            p.setColor(Color.rgb(20, 14, 40));
            c.drawRoundRect(bar, bh / 2, bh / 2, p);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3 * u); p.setColor(Color.rgb(170, 160, 230));
            c.drawRoundRect(bar, bh / 2, bh / 2, p);
            p.setStyle(Paint.Style.FILL);
            if (progress > 0.01f) {
                RectF f = new RectF(bar.left + 4 * u, bar.top + 4 * u, bar.left + 4 * u + (bar.width() - 8 * u) * progress, bar.bottom - 4 * u);
                p.setShader(new LinearGradient(0, f.top, 0, f.bottom, Color.rgb(130, 245, 90), Color.rgb(50, 190, 40), Shader.TileMode.CLAMP));
                c.drawRoundRect(f, f.height() / 2, f.height() / 2, p);
                p.setShader(null);
                c.save(); c.clipRect(f);
                p.setColor(Color.argb(60, 255, 255, 255));
                float sw = bh, shift = ((SystemClock.uptimeMillis() - t0) / 18f) % (sw * 2);
                for (float x = f.left - sw * 2 + shift; x < f.right + sw; x += sw * 2) {
                    Path path = new Path();
                    path.moveTo(x, f.bottom); path.lineTo(x + sw, f.bottom); path.lineTo(x + sw * 2, f.top); path.lineTo(x + sw, f.top); path.close();
                    c.drawPath(path, p);
                }
                c.restore();
            }
            p.setTextSize(22 * u);
            p.setColor(Color.rgb(255, 205, 60));
            String head = I18n.t("DICA:") + "  ";
            String tip = I18n.t(this.tip);
            p.setTextAlign(Paint.Align.LEFT);
            float tw = p.measureText(head);
            Paint p2 = new Paint(p);
            p2.setColor(Color.WHITE);
            float total = tw + p2.measureText(tip);
            float x0 = w / 2f - total / 2;
            c.drawText(head, x0, h * 0.86f, p);
            c.drawText(tip, x0 + tw, h * 0.86f, p2);
            postInvalidateOnAnimation();
        }
    }
}
