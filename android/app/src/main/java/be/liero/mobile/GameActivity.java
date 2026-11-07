package be.liero.mobile;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;

/** Hosts the original OpenLiero C++ game (libmain.so) through SDL2. */
public class GameActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String[] getArguments() {
        Prefs p = new Prefs(this);
        ArrayList<String> a = new ArrayList<>();
        a.add("--data-dir"); a.add(AssetInstaller.dataDir(this).getAbsolutePath());
        if (getIntent().getBooleanExtra("autostart", true)) a.add("--autostart");
        a.add("--difficulty"); a.add(String.valueOf(p.difficulty()));
        a.add("--name"); a.add(p.name());
        a.add("--lives"); a.add(String.valueOf(p.lives()));
        a.add("--new-map"); a.add(p.newMap() ? "1" : "0");
        a.add("--touch-scale"); a.add(String.valueOf(p.touchScale() / 100f));
        a.add("--touch-opacity"); a.add(String.valueOf(p.touchOpacity() / 100f));
        a.add("--touch-lefty"); a.add(p.lefty() ? "1" : "0");
        return a.toArray(new String[0]);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
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

    @Override
    public void onBackPressed() {
        // Never close the game with Back: SDL forwards it to the game, which
        // opens the original pause menu (RESUME GAME / QUIT).
    }

    @Override
    protected void onDestroy() {
        boolean finishing = isFinishing();
        super.onDestroy();
        if (finishing) {
            // OpenLiero keeps global state; end this (":game") process so the
            // next match starts clean. The launcher lives in the main process.
            System.exit(0);
        }
    }
}
