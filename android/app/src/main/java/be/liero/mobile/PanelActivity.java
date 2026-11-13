package be.liero.mobile;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;

/** Base for the launcher's compact panels (Personalizar, Mapas, Estatísticas, Conquistas). */
abstract class PanelActivity extends Activity {
    PanelView panel;
    Prefs prefs;
    /** >= 0 while waiting for a gamepad button for that action. */
    int capturing = -1;

    abstract PanelView build();

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        LauncherActivity.font(this);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        prefs = new Prefs(this);
        panel = build();
        setContentView(panel);
        hideSystemUi();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    void hideSystemUi() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (capturing >= 0 && e.getAction() == KeyEvent.ACTION_DOWN) {
            int code = Prefs.padCode(e.getKeyCode());
            if (code >= 0) {
                onCaptured(capturing, code);
                capturing = -1;
                panel.overlay = null;
                panel.invalidate();
                return true;
            }
            if (e.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                capturing = -1; panel.overlay = null; panel.invalidate(); return true;
            }
            return true;
        }
        if (e.getAction() == KeyEvent.ACTION_UP && (e.getKeyCode() == KeyEvent.KEYCODE_BACK || e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B)) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(e);
    }

    void onCaptured(int action, int code) {}

    static String pct(int v) { return v + "%"; }

    static int cycle(int v, int d, int[] opts) {
        int i = 0;
        for (int k = 0; k < opts.length; ++k) if (opts[k] == v) i = k;
        return opts[Math.max(0, Math.min(opts.length - 1, i + d))];
    }
}
