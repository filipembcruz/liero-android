package be.liero.mobile;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.view.MotionEvent;

import java.util.ArrayList;
import java.util.List;
import java.util.function.IntConsumer;
import java.util.function.Supplier;

/**
 * Compact settings panel drawn exactly like the in-game one (menu.cpp):
 * dimmed scenery, centred panel, title with icon, up to 4 tabs, rows with
 * ◀ value ▶ steppers and a VOLTAR button. Everything fits on one screen.
 */
class PanelView extends LauncherActivity.SceneView {

    /** One line in the panel. */
    static class Row {
        static final int STEP = 0, STAT = 1, NOTE = 2, PAD = 3, BUTTON = 4, EDIT = 5;
        int kind;
        String label, sub;
        Supplier<String> value;
        IntConsumer step;     // STEP: -1/+1; PAD/BUTTON/EDIT: 0 on tap
        boolean good;         // STAT: highlight value in yellow
        int icon;
        RectF r = new RectF(), left = new RectF(), right = new RectF();
    }

    interface Content { List<Row> rows(int tab); }

    static Row step(String label, String sub, Supplier<String> value, IntConsumer step) {
        Row r = new Row(); r.kind = Row.STEP; r.label = label; r.sub = sub; r.value = value; r.step = step; return r;
    }
    static Row stat(String label, String sub, String value, boolean good) {
        Row r = new Row(); r.kind = Row.STAT; r.label = label; r.sub = sub; r.value = () -> value; r.good = good; return r;
    }
    static Row note(String text) { Row r = new Row(); r.kind = Row.NOTE; r.label = text; return r; }
    static Row pad(String label, Supplier<String> value, IntConsumer tap) {
        Row r = new Row(); r.kind = Row.PAD; r.label = label; r.value = value; r.step = tap; return r;
    }
    static Row button(String label, int icon, IntConsumer tap) {
        Row r = new Row(); r.kind = Row.BUTTON; r.label = label; r.icon = icon; r.step = tap; return r;
    }
    static Row edit(String label, String sub, Supplier<String> value, IntConsumer tap) {
        Row r = new Row(); r.kind = Row.EDIT; r.label = label; r.sub = sub; r.value = value; r.step = tap; return r;
    }

    final String title;
    final int titleIcon;
    final String[] tabs;
    final Content content;
    final Runnable onBack;
    int tab;
    String overlay; // modal message (e.g. "Aperte o botão para: Atirar")

    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF backR = new RectF();
    private final RectF[] tabR = { new RectF(), new RectF(), new RectF(), new RectF(), new RectF() };
    private List<Row> rows = new ArrayList<>();
    private Object downOn; // what the finger went down on
    private int downDir;

    PanelView(Context c, String title, int titleIcon, String[] tabs, int tab, Content content, Runnable onBack) {
        super(c);
        this.title = title; this.titleIcon = titleIcon; this.tabs = tabs; this.tab = tab;
        this.content = content; this.onBack = onBack;
        p.setTypeface(LauncherActivity.font(c));
    }

    // -------------------------------------------------------------- drawing

    private void round(Canvas c, RectF r, float rad, int top, int bot) {
        p.setStyle(Paint.Style.FILL);
        if (top == bot) { p.setShader(null); p.setColor(top); }
        else p.setShader(new LinearGradient(0, r.top, 0, r.bottom, top, bot, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, rad, rad, p);
        p.setShader(null);
    }

    private void outline(Canvas c, RectF r, float rad, float t, int col) {
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(t); p.setColor(col);
        c.drawRoundRect(r, rad, rad, p);
        p.setStyle(Paint.Style.FILL);
    }

    /** Text vertically centred on cy; shrinks to fit maxW. */
    private void text(Canvas c, String s, float x, float cy, float size, int col, Paint.Align a, float maxW, boolean shadow) {
        p.setTextSize(size);
        if (maxW > 0) {
            float tw = p.measureText(s);
            if (tw > maxW) p.setTextSize(size * maxW / tw);
        }
        p.setTextAlign(a);
        Paint.FontMetrics fm = p.getFontMetrics();
        float y = cy - (fm.ascent + fm.descent) / 2;
        if (shadow) { p.setColor(Color.argb(150, 0, 0, 0)); c.drawText(s, x + 2 * u, y + 2 * u, p); }
        p.setColor(col);
        c.drawText(s, x, y, p);
    }

    private void arrowBtn(Canvas c, RectF r, boolean leftArrow, boolean pressed) {
        round(c, r, 10 * u, pressed ? Color.rgb(130, 106, 230) : Color.rgb(96, 72, 190), Color.rgb(64, 44, 140));
        outline(c, r, 10 * u, 2.5f * u, Color.rgb(170, 150, 255));
        float cx = r.centerX(), cy = r.centerY(), s = r.height() * 0.22f;
        android.graphics.Path t = new android.graphics.Path();
        if (leftArrow) { t.moveTo(cx - s, cy); t.lineTo(cx + s * 0.8f, cy - s); t.lineTo(cx + s * 0.8f, cy + s); }
        else { t.moveTo(cx + s, cy); t.lineTo(cx - s * 0.8f, cy - s); t.lineTo(cx - s * 0.8f, cy + s); }
        t.close();
        p.setColor(Color.WHITE);
        c.drawPath(t, p);
    }

    private void bigButton(Canvas c, RectF r, String label, int icon, boolean primary, boolean pressed) {
        LauncherActivity.Btn b = new LauncherActivity.Btn(0, label, icon, primary, false, false);
        b.r.set(r);
        Icons.button(c, p, b, pressed, u * 0.8f, p.getTypeface());
    }

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        drawScene(c, false);
        p.setShader(null);
        p.setColor(Color.argb(200, 8, 10, 26));
        c.drawRect(0, 0, w, h, p);

        float pw = Math.min(1120 * u, w - 40 * u);
        RectF pnl = new RectF((w - pw) / 2, 30 * u, (w + pw) / 2, h - 30 * u);
        // panel (same as ui::panel)
        p.setColor(Color.argb(110, 70, 120, 255));
        c.drawRoundRect(new RectF(pnl.left - 4 * u, pnl.top - 4 * u, pnl.right + 4 * u, pnl.bottom + 4 * u), 26 * u, 26 * u, p);
        round(c, pnl, 22 * u, Color.argb(240, 22, 30, 60), Color.argb(240, 12, 16, 36));
        outline(c, pnl, 22 * u, 3 * u, Color.rgb(90, 140, 230));

        // title
        Icons.draw(c, p, titleIcon, pnl.left + 44 * u, pnl.top + 46 * u, 18 * u, Color.rgb(255, 215, 90));
        float tabsW = tabs.length > 1 ? tabs.length * 176 * u : 0;
        text(c, title, pnl.left + 76 * u, pnl.top + 46 * u, 34 * u, Color.WHITE, Paint.Align.LEFT,
            pnl.width() - 110 * u - tabsW, true);

        // tabs (right aligned)
        if (tabs.length > 1) {
            for (int i = 0; i < tabs.length; ++i) {
                RectF t = tabR[i];
                t.set(pnl.right - 24 * u - (tabs.length - i) * 176 * u + 10 * u, pnl.top + 18 * u,
                    pnl.right - 24 * u - (tabs.length - i - 1) * 176 * u, pnl.top + 72 * u);
                boolean sel = i == tab;
                round(c, t, 14 * u, sel ? Color.rgb(255, 214, 70) : Color.rgb(60, 50, 120), sel ? Color.rgb(240, 160, 20) : Color.rgb(40, 32, 90));
                text(c, tabs[i], t.centerX(), t.centerY(), 24 * u, sel ? Color.rgb(60, 30, 10) : Color.WHITE,
                    Paint.Align.CENTER, t.width() - 20 * u, !sel);
            }
        }

        // rows
        rows = content.rows(tab);
        float y = pnl.top + 90 * u, rh = 62 * u, gap = 6 * u;
        float x0 = pnl.left + 28 * u, x1 = pnl.right - 28 * u;
        int padIdx = 0;
        int padCount = 0;
        for (Row r : rows) if (r.kind == Row.PAD) padCount++;
        int padPerCol = (padCount + 1) / 2;
        float padTop = 0;
        for (Row r : rows) {
            switch (r.kind) {
                case Row.NOTE:
                    text(c, r.label, x0 + 2 * u, y + 16 * u, 18 * u, Color.rgb(170, 185, 240), Paint.Align.LEFT, x1 - x0, false);
                    y += 34 * u;
                    break;
                case Row.PAD: {
                    if (padIdx == 0) padTop = y;
                    float colW = (x1 - x0 - 14 * u) / 2;
                    int col = padIdx / padPerCol, line = padIdx % padPerCol;
                    r.r.set(x0 + col * (colW + 14 * u), padTop + line * (rh + gap), x0 + col * (colW + 14 * u) + colW, padTop + line * (rh + gap) + rh);
                    boolean pr = downOn == r;
                    round(c, r.r, 12 * u, Color.argb(pr ? 240 : 180, 40, 50, 90), Color.argb(pr ? 240 : 180, 40, 50, 90));
                    RectF chip = new RectF(r.r.right - 210 * u, r.r.top + rh * 0.18f, r.r.right - 20 * u, r.r.bottom - rh * 0.18f);
                    text(c, r.label, r.r.left + 20 * u, r.r.centerY(), 23 * u, Color.WHITE, Paint.Align.LEFT, chip.left - r.r.left - 30 * u, false);
                    round(c, chip, 10 * u, Color.rgb(96, 72, 190), Color.rgb(64, 44, 140));
                    outline(c, chip, 10 * u, 2 * u, Color.rgb(170, 150, 255));
                    text(c, r.value.get(), chip.centerX(), chip.centerY(), 21 * u, Color.rgb(255, 215, 90), Paint.Align.CENTER, chip.width() - 16 * u, false);
                    padIdx++;
                    if (padIdx == padCount) y = padTop + padPerCol * (rh + gap);
                    break;
                }
                case Row.BUTTON:
                    r.r.set(x0, y + 4 * u, x0 + 330 * u, y + 60 * u);
                    bigButton(c, r.r, r.label, r.icon, false, downOn == r);
                    y += rh + gap;
                    break;
                default: {
                    r.r.set(x0, y, x1, y + rh);
                    round(c, r.r, 12 * u, Color.argb(180, 40, 50, 90), Color.argb(180, 40, 50, 90));
                    float valueW = 380 * u;
                    float labelMax = r.r.width() - valueW - 40 * u;
                    if (r.sub != null) {
                        text(c, r.label, r.r.left + 20 * u, r.r.top + rh * 0.36f, 23 * u, Color.WHITE, Paint.Align.LEFT, labelMax, false);
                        text(c, r.sub, r.r.left + 20 * u, r.r.top + rh * 0.74f, 15 * u, Color.rgb(160, 175, 230), Paint.Align.LEFT, labelMax, false);
                    } else {
                        text(c, r.label, r.r.left + 20 * u, r.r.centerY(), 23 * u, Color.WHITE, Paint.Align.LEFT, labelMax, false);
                    }
                    float vcx = r.r.right - 18 * u - valueW / 2;
                    if (r.kind == Row.STEP) {
                        float bs = rh - 14 * u;
                        r.left.set(r.r.right - 18 * u - valueW, r.r.top + 7 * u, r.r.right - 18 * u - valueW + bs, r.r.bottom - 7 * u);
                        r.right.set(r.r.right - 18 * u - bs, r.r.top + 7 * u, r.r.right - 18 * u, r.r.bottom - 7 * u);
                        arrowBtn(c, r.left, true, downOn == r && downDir < 0);
                        arrowBtn(c, r.right, false, downOn == r && downDir > 0);
                        text(c, r.value.get(), vcx, r.r.centerY(), 23 * u, Color.rgb(255, 215, 90), Paint.Align.CENTER, valueW - 2 * bs - 20 * u, false);
                    } else if (r.kind == Row.EDIT) {
                        RectF chip = new RectF(r.r.right - 18 * u - valueW, r.r.top + 9 * u, r.r.right - 18 * u, r.r.bottom - 9 * u);
                        round(c, chip, 10 * u, Color.argb(downOn == r ? 255 : 220, 18, 22, 44), Color.argb(downOn == r ? 255 : 220, 18, 22, 44));
                        outline(c, chip, 10 * u, 2 * u, Color.rgb(170, 150, 255));
                        text(c, r.value.get(), vcx, r.r.centerY(), 22 * u, Color.rgb(255, 215, 90), Paint.Align.CENTER, chip.width() - 24 * u, false);
                    } else {
                        text(c, r.value.get(), vcx, r.r.centerY(), 22 * u, r.good ? Color.rgb(255, 215, 90) : Color.rgb(150, 160, 200),
                            Paint.Align.CENTER, valueW, false);
                    }
                    y += rh + gap;
                }
            }
        }

        backR.set(pnl.right - 280 * u, pnl.bottom - 72 * u, pnl.right - 30 * u, pnl.bottom - 14 * u);
        bigButton(c, backR, "VOLTAR", Icons.BACK, true, downOn == backR);

        if (overlay != null) {
            p.setColor(Color.argb(170, 0, 0, 0));
            c.drawRect(0, 0, w, h, p);
            RectF m = new RectF(w / 2f - 380 * u, h / 2f - 110 * u, w / 2f + 380 * u, h / 2f + 110 * u);
            round(c, m, 22 * u, Color.argb(250, 22, 30, 60), Color.argb(250, 12, 16, 36));
            outline(c, m, 22 * u, 3 * u, Color.rgb(90, 140, 230));
            text(c, overlay, m.centerX(), m.centerY() - 22 * u, 30 * u, Color.WHITE, Paint.Align.CENTER, m.width() - 40 * u, true);
            text(c, "Toque na tela para cancelar", m.centerX(), m.centerY() + 36 * u, 20 * u, Color.rgb(170, 185, 240), Paint.Align.CENTER, 0, false);
        }
    }

    // -------------------------------------------------------------- input

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        float x = e.getX(), y = e.getY();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downOn = null; downDir = 0;
                if (overlay != null) { overlay = null; invalidate(); onOverlayCancelled(); return true; }
                if (backR.contains(x, y)) downOn = backR;
                for (int i = 0; i < tabs.length && tabs.length > 1; ++i)
                    if (tabR[i].contains(x, y)) downOn = tabR[i];
                for (Row r : rows) {
                    if (r.kind == Row.STEP) {
                        RectF l = new RectF(r.left), rr = new RectF(r.right);
                        l.inset(-10 * u, -6 * u); rr.inset(-10 * u, -6 * u);
                        if (l.contains(x, y)) { downOn = r; downDir = -1; }
                        else if (rr.contains(x, y)) { downOn = r; downDir = 1; }
                    } else if ((r.kind == Row.PAD || r.kind == Row.BUTTON || r.kind == Row.EDIT) && r.r.contains(x, y)) {
                        downOn = r;
                    }
                }
                invalidate();
                return true;
            case MotionEvent.ACTION_UP: {
                Object d = downOn;
                downOn = null;
                if (d == backR && backR.contains(x, y)) { onBack.run(); }
                for (int i = 0; i < tabs.length; ++i) if (d == tabR[i] && tabR[i].contains(x, y)) tab = i;
                if (d instanceof Row) {
                    Row r = (Row) d;
                    if (r.kind == Row.STEP) r.step.accept(downDir);
                    else if (r.r.contains(x, y)) r.step.accept(0);
                    performHapticFeedback(android.view.HapticFeedbackConstants.VIRTUAL_KEY);
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                downOn = null; invalidate(); return true;
        }
        return true;
    }

    void onOverlayCancelled() {}
}
