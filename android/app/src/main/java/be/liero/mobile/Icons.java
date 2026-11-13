package be.liero.mobile;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

/** Vector icons and the chunky game-style buttons of the start screen. */
final class Icons {
    static final int PLAY = 1, PEOPLE = 2, TOOLS = 3, MAP = 4, GEAR = 5, BARS = 6, TROPHY = 7, BACK = 8, RESTART = 9;

    static void button(Canvas c, Paint p, LauncherActivity.Btn b, boolean pressed, float u, Typeface font) {
        RectF r = new RectF(b.r);
        float rad = b.square ? 16 * u : r.height() * 0.28f;
        float dy = pressed ? 4 * u : 0;
        p.setShader(null);
        p.setStyle(Paint.Style.FILL);
        // drop shadow / dark outline
        p.setColor(Color.argb(200, 14, 8, 30));
        c.drawRoundRect(new RectF(r.left - 3 * u, r.top - 3 * u + 6 * u, r.right + 3 * u, r.bottom + 3 * u + 6 * u), rad + 3 * u, rad + 3 * u, p);
        r.offset(0, dy);
        c.drawRoundRect(new RectF(r.left - 3 * u, r.top - 3 * u, r.right + 3 * u, r.bottom + 3 * u), rad + 3 * u, rad + 3 * u, p);
        int top, bot, rim, text;
        if (b.primary) { top = Color.rgb(255, 222, 80); bot = Color.rgb(242, 160, 20); rim = Color.rgb(255, 244, 180); text = Color.rgb(60, 28, 8); }
        else { top = Color.rgb(112, 84, 214); bot = Color.rgb(70, 48, 150); rim = Color.rgb(170, 150, 255); text = Color.WHITE; }
        if (b.disabled) { top = Color.rgb(70, 62, 110); bot = Color.rgb(50, 44, 84); rim = Color.rgb(100, 94, 140); text = Color.rgb(150, 145, 180); }
        p.setShader(new LinearGradient(0, r.top, 0, r.bottom, top, bot, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, rad, rad, p);
        p.setShader(null);
        // gloss
        p.setColor(Color.argb(55, 255, 255, 255));
        c.drawRoundRect(new RectF(r.left + 8 * u, r.top + 5 * u, r.right - 8 * u, r.top + r.height() * 0.42f), rad * 0.7f, rad * 0.7f, p);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(3 * u);
        p.setColor(rim);
        c.drawRoundRect(r, rad, rad, p);
        p.setStyle(Paint.Style.FILL);
        if (pressed) { p.setColor(Color.argb(40, 255, 255, 255)); c.drawRoundRect(r, rad, rad, p); }

        if (b.label == null) {
            draw(c, p, b.icon, r.centerX(), r.centerY(), r.height() * 0.32f, Color.WHITE);
            return;
        }
        p.setTypeface(font);
        float ts = r.height() * (b.primary ? 0.42f : 0.38f);
        p.setTextSize(ts);
        p.setTextAlign(Paint.Align.LEFT);
        float tw = p.measureText(b.label);
        float is = r.height() * 0.26f;
        float total = tw + is * 2.6f;
        float avail = r.width() - 2 * Math.max(18 * u, r.height() * 0.3f);
        if (total > avail) {
            float k = avail / total;
            ts *= k; is *= k; p.setTextSize(ts); tw = p.measureText(b.label); total = tw + is * 2.6f;
        }
        float x0 = r.centerX() - total / 2;
        draw(c, p, b.icon, x0 + is, r.centerY(), is, text);
        p.setColor(text);
        Paint.FontMetrics fm = p.getFontMetrics();
        c.drawText(b.label, x0 + is * 2.6f, r.centerY() - (fm.ascent + fm.descent) / 2, p);
        if (b.disabled) {
            // "EM BREVE" tag
            p.setTextSize(r.height() * 0.24f);
            String t = "EM BREVE";
            float w = p.measureText(t) + 20 * u, h = r.height() * 0.36f;
            RectF tag = new RectF(r.right - w - 10 * u, r.top - h * 0.55f, r.right - 10 * u, r.top + h * 0.45f);
            p.setColor(Color.rgb(255, 205, 60));
            c.drawRoundRect(tag, h / 2, h / 2, p);
            p.setColor(Color.rgb(60, 30, 10));
            p.setTextAlign(Paint.Align.CENTER);
            Paint.FontMetrics f2 = p.getFontMetrics();
            c.drawText(t, tag.centerX(), tag.centerY() - (f2.ascent + f2.descent) / 2, p);
        }
    }

    static void draw(Canvas c, Paint p, int icon, float x, float y, float s, int color) {
        p.setShader(null);
        p.setColor(color);
        p.setStyle(Paint.Style.FILL);
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeJoin(Paint.Join.ROUND);
        Path path = new Path();
        switch (icon) {
            case PLAY:
                path.moveTo(x - s * 0.6f, y - s * 0.85f); path.lineTo(x + s * 0.85f, y); path.lineTo(x - s * 0.6f, y + s * 0.85f); path.close();
                c.drawPath(path, p);
                break;
            case PEOPLE:
                c.drawCircle(x, y - s * 0.35f, s * 0.35f, p);
                c.drawRoundRect(new RectF(x - s * 0.6f, y + s * 0.1f, x + s * 0.6f, y + s * 0.85f), s * 0.4f, s * 0.4f, p);
                c.drawCircle(x - s * 0.85f, y - s * 0.2f, s * 0.26f, p);
                c.drawCircle(x + s * 0.85f, y - s * 0.2f, s * 0.26f, p);
                c.drawRoundRect(new RectF(x - s * 1.25f, y + s * 0.15f, x - s * 0.5f, y + s * 0.7f), s * 0.3f, s * 0.3f, p);
                c.drawRoundRect(new RectF(x + s * 0.5f, y + s * 0.15f, x + s * 1.25f, y + s * 0.7f), s * 0.3f, s * 0.3f, p);
                break;
            case TOOLS:
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.32f);
                c.drawLine(x - s * 0.75f, y + s * 0.75f, x + s * 0.55f, y - s * 0.55f, p);
                c.drawLine(x - s * 0.75f, y - s * 0.75f, x + s * 0.75f, y + s * 0.75f, p);
                p.setStyle(Paint.Style.FILL);
                c.drawCircle(x + s * 0.6f, y - s * 0.6f, s * 0.3f, p);
                break;
            case MAP:
                path.moveTo(x - s * 0.9f, y - s * 0.6f); path.lineTo(x - s * 0.3f, y - s * 0.85f); path.lineTo(x + s * 0.3f, y - s * 0.6f);
                path.lineTo(x + s * 0.9f, y - s * 0.85f); path.lineTo(x + s * 0.9f, y + s * 0.6f); path.lineTo(x + s * 0.3f, y + s * 0.85f);
                path.lineTo(x - s * 0.3f, y + s * 0.6f); path.lineTo(x - s * 0.9f, y + s * 0.85f); path.close();
                c.drawPath(path, p);
                p.setColor(Color.argb(120, 0, 0, 0)); p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.1f);
                c.drawLine(x - s * 0.3f, y - s * 0.85f, x - s * 0.3f, y + s * 0.6f, p);
                c.drawLine(x + s * 0.3f, y - s * 0.6f, x + s * 0.3f, y + s * 0.85f, p);
                break;
            case GEAR:
                for (int i = 0; i < 8; ++i) {
                    double a = i * Math.PI / 4;
                    c.save(); c.rotate((float) Math.toDegrees(a), x, y);
                    c.drawRoundRect(new RectF(x - s * 0.2f, y - s * 1.0f, x + s * 0.2f, y - s * 0.55f), s * 0.06f, s * 0.06f, p);
                    c.restore();
                }
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.38f);
                c.drawCircle(x, y, s * 0.5f, p);
                p.setStyle(Paint.Style.FILL);
                break;
            case BARS:
                c.drawRoundRect(new RectF(x - s * 0.9f, y - s * 0.1f, x - s * 0.4f, y + s * 0.85f), s * 0.08f, s * 0.08f, p);
                c.drawRoundRect(new RectF(x - s * 0.25f, y - s * 0.85f, x + s * 0.25f, y + s * 0.85f), s * 0.08f, s * 0.08f, p);
                c.drawRoundRect(new RectF(x + s * 0.4f, y - s * 0.45f, x + s * 0.9f, y + s * 0.85f), s * 0.08f, s * 0.08f, p);
                break;
            case TROPHY:
                p.setColor(Color.rgb(255, 200, 50));
                path.moveTo(x - s * 0.7f, y - s * 0.85f); path.lineTo(x + s * 0.7f, y - s * 0.85f);
                path.quadTo(x + s * 0.7f, y + s * 0.15f, x, y + s * 0.25f);
                path.quadTo(x - s * 0.7f, y + s * 0.15f, x - s * 0.7f, y - s * 0.85f); path.close();
                c.drawPath(path, p);
                c.drawRect(x - s * 0.12f, y + s * 0.2f, x + s * 0.12f, y + s * 0.55f, p);
                c.drawRoundRect(new RectF(x - s * 0.5f, y + s * 0.55f, x + s * 0.5f, y + s * 0.85f), s * 0.08f, s * 0.08f, p);
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.14f);
                c.drawArc(new RectF(x - s * 1.05f, y - s * 0.75f, x - s * 0.45f, y - s * 0.1f), 90, 180, false, p);
                c.drawArc(new RectF(x + s * 0.45f, y - s * 0.75f, x + s * 1.05f, y - s * 0.1f), 270, 180, false, p);
                break;
            case BACK:
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.26f);
                c.drawLine(x - s * 0.75f, y, x + s * 0.75f, y, p);
                c.drawLine(x - s * 0.75f, y, x - s * 0.15f, y - s * 0.6f, p);
                c.drawLine(x - s * 0.75f, y, x - s * 0.15f, y + s * 0.6f, p);
                break;
            case RESTART: {
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(s * 0.26f);
                float R = s * 0.72f;
                c.drawArc(new RectF(x - R, y - R, x + R, y + R), -45, 280, false, p);
                p.setStyle(Paint.Style.FILL);
                double a = Math.toRadians(235);
                float ex = x + (float) Math.cos(a) * R, ey = y + (float) Math.sin(a) * R;
                float tx = (float) -Math.sin(a), ty = (float) Math.cos(a), nx = (float) Math.cos(a), ny = (float) Math.sin(a);
                float L = s * 0.55f, W = s * 0.45f;
                path.moveTo(ex + tx * L * 0.75f, ey + ty * L * 0.75f);
                path.lineTo(ex + nx * W - tx * L * 0.25f, ey + ny * W - ty * L * 0.25f);
                path.lineTo(ex - nx * W - tx * L * 0.25f, ey - ny * W - ty * L * 0.25f);
                path.close();
                c.drawPath(path, p);
                break;
            }
        }
        p.setStyle(Paint.Style.FILL);
    }
}
