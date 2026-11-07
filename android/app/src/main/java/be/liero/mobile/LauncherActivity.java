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

/**
 * Start screen (artwork from the project brief): JOGAR, PERSONALIZAR, MAPAS,
 * settings gear, stats and trophies. Multiplayer is shown but disabled for
 * now (single player only). JOGAR shows the loading screen while the game
 * data is installed, then starts the original game.
 */
public class LauncherActivity extends Activity {

    private MenuView menu;
    private LoadingView loading;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private boolean starting;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        menu = new MenuView(this);
        setContentView(menu);
        hideSystemUi();
        // Install data in the background right away so JOGAR is instant.
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
        showLoading(0f);
        final long t0 = SystemClock.uptimeMillis();
        new Thread(() -> {
            try {
                AssetInstaller.install(this, p -> ui.post(() -> loading.setProgress(p * 0.85f)));
            } catch (Exception e) {
                ui.post(() -> {
                    Toast.makeText(this, "Erro ao instalar dados: " + e.getMessage(), Toast.LENGTH_LONG).show();
                    setContentView(menu);
                    starting = false;
                });
                return;
            }
            // Short, smooth finish of the bar (data install is usually instant).
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

    // Maps a region of the 1672x941 artwork to an action.
    static final class Zone {
        final RectF r; final int id; final boolean disabled;
        Zone(float l, float t, float rr, float b, int id, boolean disabled) {
            this.r = new RectF(l, t, rr, b); this.id = id; this.disabled = disabled;
        }
    }

    static final int Z_PLAY = 1, Z_MULTI = 2, Z_CUSTOM = 3, Z_MAPS = 4, Z_GEAR = 5, Z_STATS = 6, Z_TROPHY = 7;

    /** Draws artwork scaled to fit (letterbox filled with a dimmed cover copy). */
    abstract static class ArtView extends View {
        final Bitmap art;
        final Matrix fit = new Matrix(), cover = new Matrix();
        final Paint bmpPaint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        final Paint dim = new Paint();
        float scale = 1, ox = 0, oy = 0;

        ArtView(Context c, int res) {
            super(c);
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inScaled = false;
            art = BitmapFactory.decodeResource(c.getResources(), res, o);
            dim.setColor(Color.argb(150, 10, 6, 24));
        }

        @Override
        protected void onSizeChanged(int w, int h, int ow, int oh) {
            float aw = art.getWidth(), ah = art.getHeight();
            scale = Math.min(w / aw, h / ah);
            ox = (w - aw * scale) / 2f; oy = (h - ah * scale) / 2f;
            fit.reset(); fit.postScale(scale, scale); fit.postTranslate(ox, oy);
            float cs = Math.max(w / aw, h / ah);
            cover.reset(); cover.postScale(cs, cs); cover.postTranslate((w - aw * cs) / 2f, (h - ah * cs) / 2f);
        }

        void drawArt(Canvas c) {
            c.drawColor(Color.rgb(20, 14, 40));
            c.drawBitmap(art, cover, bmpPaint);
            c.drawRect(0, 0, getWidth(), getHeight(), dim);
            c.drawBitmap(art, fit, bmpPaint);
        }

        RectF toScreen(RectF a) {
            return new RectF(ox + a.left * scale, oy + a.top * scale, ox + a.right * scale, oy + a.bottom * scale);
        }
    }

    final class MenuView extends ArtView {
        final Zone[] zones = {
            new Zone(578, 366, 1095, 500, Z_PLAY, false),
            new Zone(605, 523, 1068, 613, Z_MULTI, true),
            new Zone(605, 632, 1068, 721, Z_CUSTOM, false),
            new Zone(605, 737, 1068, 826, Z_MAPS, false),
            new Zone(25, 22, 125, 108, Z_GEAR, false),
            new Zone(1418, 22, 1518, 108, Z_STATS, false),
            new Zone(1548, 22, 1648, 108, Z_TROPHY, false),
        };
        Zone pressed;
        final Paint hl = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Paint off = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Paint tag = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Paint tagText = new Paint(Paint.ANTI_ALIAS_FLAG);

        MenuView(Context c) {
            super(c, R.drawable.menu_bg);
            hl.setColor(Color.argb(70, 255, 255, 255));
            off.setColor(Color.argb(140, 12, 8, 30));
            tag.setColor(Color.rgb(255, 201, 60));
            tagText.setColor(Color.rgb(40, 20, 10));
            tagText.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
            tagText.setTextAlign(Paint.Align.CENTER);
        }

        @Override
        protected void onDraw(Canvas c) {
            drawArt(c);
            for (Zone z : zones) {
                if (!z.disabled) continue;
                RectF r = toScreen(z.r);
                float rad = r.height() * 0.22f;
                c.drawRoundRect(r, rad, rad, off);
                // "EM BREVE" tag
                float th = r.height() * 0.34f;
                tagText.setTextSize(th * 0.62f);
                float tw = tagText.measureText("EM BREVE") + th * 0.8f;
                RectF t = new RectF(r.right - tw - th * 0.3f, r.top - th * 0.45f, r.right - th * 0.3f, r.top + th * 0.55f);
                c.drawRoundRect(t, th * 0.3f, th * 0.3f, tag);
                c.drawText("EM BREVE", t.centerX(), t.centerY() + th * 0.22f, tagText);
            }
            if (pressed != null && !pressed.disabled) {
                RectF r = toScreen(pressed.r);
                float rad = r.height() * 0.22f;
                c.drawRoundRect(r, rad, rad, hl);
            }
        }

        Zone hit(float x, float y) {
            for (Zone z : zones) {
                RectF r = toScreen(z.r);
                r.inset(-r.height() * 0.08f, -r.height() * 0.08f);
                if (r.contains(x, y)) return z;
            }
            return null;
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    pressed = hit(e.getX(), e.getY());
                    if (pressed != null) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                    invalidate();
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if (pressed != null && hit(e.getX(), e.getY()) != pressed) { pressed = null; invalidate(); }
                    return true;
                case MotionEvent.ACTION_UP:
                    Zone z = pressed;
                    pressed = null;
                    invalidate();
                    if (z != null && hit(e.getX(), e.getY()) == z) onZone(z);
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    pressed = null; invalidate();
                    return true;
            }
            return super.onTouchEvent(e);
        }
    }

    void onZone(Zone z) {
        switch (z.id) {
            case Z_PLAY: play(); break;
            case Z_MULTI:
                Toast.makeText(this, "Multijogador chega numa próxima versão. Por enquanto: você contra a CPU.", Toast.LENGTH_SHORT).show();
                break;
            case Z_CUSTOM: case Z_GEAR: startActivity(new Intent(this, SettingsActivity.class)); break;
            case Z_MAPS: startActivity(new Intent(this, MapsActivity.class)); break;
            case Z_STATS: startActivity(new Intent(this, StatsActivity.class)); break;
            case Z_TROPHY: startActivity(new Intent(this, StatsActivity.class).putExtra("trophies", true)); break;
        }
    }

    /** Loading screen artwork with a live progress bar over the drawn one. */
    static final class LoadingView extends ArtView {
        float progress;
        final RectF bar = new RectF(398, 748, 1272, 786); // inner area of the bar in the artwork
        final Paint back = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Paint stripe = new Paint(Paint.ANTI_ALIAS_FLAG);
        final long start = SystemClock.uptimeMillis();

        LoadingView(Context c) {
            super(c, R.drawable.loading_bg);
            back.setColor(Color.rgb(26, 20, 44));
            stripe.setColor(Color.argb(70, 255, 255, 255));
        }

        void setProgress(float p) { progress = Math.max(0, Math.min(1, p)); invalidate(); }

        @Override
        protected void onDraw(Canvas c) {
            drawArt(c);
            RectF r = toScreen(bar);
            float rad = r.height() * 0.3f;
            c.drawRoundRect(r, rad, rad, back);
            if (progress > 0.001f) {
                RectF f = new RectF(r.left, r.top, r.left + r.width() * progress, r.bottom);
                fill.setShader(new LinearGradient(0, f.top, 0, f.bottom,
                    Color.rgb(120, 240, 90), Color.rgb(50, 190, 40), Shader.TileMode.CLAMP));
                c.drawRoundRect(f, rad, rad, fill);
                c.save();
                c.clipRect(f);
                float sw = r.height() * 0.7f;
                float shift = ((SystemClock.uptimeMillis() - start) / 18f) % (sw * 2);
                for (float x = f.left - sw * 2 + shift; x < f.right + sw; x += sw * 2) {
                    android.graphics.Path p = new android.graphics.Path();
                    p.moveTo(x, f.bottom); p.lineTo(x + sw, f.bottom);
                    p.lineTo(x + sw * 2, f.top); p.lineTo(x + sw, f.top); p.close();
                    c.drawPath(p, stripe);
                }
                c.restore();
            }
            postInvalidateOnAnimation();
        }
    }
}
