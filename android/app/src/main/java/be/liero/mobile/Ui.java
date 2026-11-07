package be.liero.mobile;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

/** Tiny programmatic UI kit in the launcher's purple/yellow pixel style. */
final class Ui {
    static final int BG = Color.rgb(28, 20, 58);
    static final int CARD = Color.rgb(52, 38, 104);
    static final int CARD_EDGE = Color.rgb(120, 96, 200);
    static final int YELLOW = Color.rgb(255, 201, 60);
    static final int TEXT = Color.rgb(240, 236, 255);
    static final int MUTED = Color.rgb(176, 166, 220);
    static final Typeface FONT = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD);

    final Activity a;
    final LinearLayout body;

    Ui(Activity a, String title) {
        this.a = a;
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = a.getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            a.getWindow().setAttributes(lp);
        }
        FrameLayout root = new FrameLayout(a);
        root.setBackgroundColor(BG);

        LinearLayout col = new LinearLayout(a);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(48), dp(14), dp(48), dp(14));

        LinearLayout top = new LinearLayout(a);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView back = button("◀  VOLTAR", false);
        back.setOnClickListener(v -> a.finish());
        top.addView(back);
        TextView t = text(title, 26, YELLOW);
        t.setPadding(dp(24), 0, 0, 0);
        top.addView(t);
        col.addView(top);

        ScrollView sv = new ScrollView(a);
        body = new LinearLayout(a);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(0, dp(10), 0, dp(24));
        sv.addView(body);
        col.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        root.addView(col);
        a.setContentView(root);
        a.getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, a.getResources().getDisplayMetrics());
    }

    TextView text(String s, float sp, int color) {
        TextView t = new TextView(a);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        t.setTypeface(FONT);
        return t;
    }

    GradientDrawable box(int fill, int edge, float radiusDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(radiusDp));
        g.setStroke(dp(2), edge);
        return g;
    }

    TextView button(String s, boolean selected) {
        TextView b = text(s, 16, selected ? Color.rgb(40, 22, 8) : TEXT);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(18), dp(10), dp(18), dp(10));
        b.setBackground(box(selected ? YELLOW : CARD, selected ? Color.rgb(255, 230, 140) : CARD_EDGE, 10));
        b.setClickable(true);
        return b;
    }

    LinearLayout card(String title, String subtitle) {
        LinearLayout c = new LinearLayout(a);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setPadding(dp(18), dp(12), dp(18), dp(14));
        c.setBackground(box(Color.argb(160, 52, 38, 104), CARD_EDGE, 14));
        c.addView(text(title, 18, TEXT));
        if (subtitle != null) {
            TextView st = text(subtitle, 13, MUTED);
            st.setTypeface(Typeface.MONOSPACE);
            st.setPadding(0, dp(2), 0, dp(8));
            c.addView(st);
        }
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(12);
        body.addView(c, lp);
        return c;
    }

    interface Choice { void pick(int i); }

    /** Row of exclusive options (segmented control). */
    LinearLayout segmented(LinearLayout parent, String[] labels, int selected, Choice cb) {
        LinearLayout row = new LinearLayout(a);
        row.setOrientation(LinearLayout.HORIZONTAL);
        TextView[] bs = new TextView[labels.length];
        for (int i = 0; i < labels.length; ++i) {
            final int idx = i;
            bs[i] = button(labels[i], i == selected);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
            lp.rightMargin = dp(8);
            row.addView(bs[i], lp);
            bs[i].setOnClickListener(v -> {
                for (int j = 0; j < bs.length; ++j) {
                    TextView n = button(labels[j], j == idx);
                    bs[j].setBackground(n.getBackground());
                    bs[j].setTextColor(n.getCurrentTextColor());
                }
                cb.pick(idx);
            });
        }
        parent.addView(row);
        return row;
    }

    interface IntCb { void set(int v); }

    SeekBar slider(LinearLayout parent, String label, int min, int max, int value, String unit, IntCb cb) {
        LinearLayout row = new LinearLayout(a);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        TextView l = text(label, 15, TEXT);
        row.addView(l, new LinearLayout.LayoutParams(dp(150), ViewGroup.LayoutParams.WRAP_CONTENT));
        SeekBar sb = new SeekBar(a);
        sb.setMax(max - min);
        sb.setProgress(value - min);
        TextView val = text(value + unit, 15, YELLOW);
        val.setGravity(Gravity.END);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean u) { val.setText((p + min) + unit); cb.set(p + min); }
            public void onStartTrackingTouch(SeekBar s) {}
            public void onStopTrackingTouch(SeekBar s) {}
        });
        row.addView(sb, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        row.addView(val, new LinearLayout.LayoutParams(dp(70), ViewGroup.LayoutParams.WRAP_CONTENT));
        parent.addView(row);
        return sb;
    }
}
