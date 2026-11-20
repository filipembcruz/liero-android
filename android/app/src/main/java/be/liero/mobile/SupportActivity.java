package be.liero.mobile;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.Build;
import android.os.Bundle;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import com.android.billingclient.api.AcknowledgePurchaseResponseListener;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * "Support an indie developer": five voluntary tips (≈ US$ 2/5/10/20/50) sold
 * as consumable Google Play in-app products (tip_2 … tip_50). They unlock
 * nothing but a heart next to the player's name.
 */
public class SupportActivity extends Activity {
    static final String[] PRODUCTS = { "tip_2", "tip_5", "tip_10", "tip_20", "tip_50" };
    static final String[] TIER = { "Um café", "Um lanche", "Uma pizza", "Grande apoio", "Mecenas" };
    static final int[] HEARTS = { 1, 2, 3, 4, 5 };
    static final int N = PRODUCTS.length;
    static final int FEATURED = 2;

    BillingClient billing;
    final ProductDetails[] details = new ProductDetails[N];
    SupportView view;
    boolean prompt;
    String status;      // message under the cards (thank you / unavailable)
    boolean thanked;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        I18n.init(this);
        LauncherActivity.font(this);
        prompt = getIntent().getBooleanExtra("prompt", false);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        view = new SupportView(this);
        setContentView(view);
        hideSystemUi();
        connect();
    }

    @Override
    protected void onDestroy() {
        if (billing != null) billing.endConnection();
        super.onDestroy();
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
        if (e.getAction() == KeyEvent.ACTION_UP && (e.getKeyCode() == KeyEvent.KEYCODE_BACK || e.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B)) {
            finish();
            return true;
        }
        return super.dispatchKeyEvent(e);
    }

    // ------------------------------------------------------------- billing

    void connect() {
        try {
            billing = BillingClient.newBuilder(this)
                .setListener((result, purchases) -> {
                    if (result.getResponseCode() == BillingClient.BillingResponseCode.OK && purchases != null)
                        for (Purchase p : purchases) handle(p);
                    else if (result.getResponseCode() != BillingClient.BillingResponseCode.USER_CANCELED)
                        setStatus(I18n.t("Não foi possível concluir. Tente de novo mais tarde."));
                })
                .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
                .build();
            billing.startConnection(new BillingClientStateListener() {
                @Override public void onBillingSetupFinished(BillingResult r) {
                    if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) { unavailable(); return; }
                    queryProducts();
                    billing.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
                        (br, list) -> { for (Purchase p : list) handle(p); });
                }
                @Override public void onBillingServiceDisconnected() {}
            });
        } catch (Throwable t) {
            unavailable();
        }
    }

    void queryProducts() {
        List<QueryProductDetailsParams.Product> list = new ArrayList<>();
        for (String id : PRODUCTS)
            list.add(QueryProductDetailsParams.Product.newBuilder().setProductId(id).setProductType(BillingClient.ProductType.INAPP).build());
        billing.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(list).build(), (r, found) -> {
            if (found != null)
                for (ProductDetails d : found)
                    for (int i = 0; i < PRODUCTS.length; ++i)
                        if (PRODUCTS[i].equals(d.getProductId())) details[i] = d;
            boolean any = false;
            for (ProductDetails d : details) any |= d != null;
            if (!any) unavailable();
            view.postInvalidate();
        });
    }

    void unavailable() {
        setStatus(I18n.t("Apoio disponível na versão da Google Play."));
    }

    void setStatus(String s) {
        runOnUiThread(() -> { status = s; view.invalidate(); });
    }

    void buy(int i) {
        if (billing == null || details[i] == null) { unavailable(); return; }
        BillingFlowParams p = BillingFlowParams.newBuilder()
            .setProductDetailsParamsList(Collections.singletonList(
                BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(details[i]).build()))
            .build();
        billing.launchBillingFlow(this, p);
    }

    void handle(Purchase p) {
        if (p.getPurchaseState() != Purchase.PurchaseState.PURCHASED) {
            if (p.getPurchaseState() == Purchase.PurchaseState.PENDING)
                setStatus(I18n.t("Pagamento pendente. Obrigado!"));
            return;
        }
        // Consumable: consuming also acknowledges it and lets the player tip again later.
        billing.consumeAsync(ConsumeParams.newBuilder().setPurchaseToken(p.getPurchaseToken()).build(), (r, token) -> {
            if (r.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                new Prefs(this).edit().putInt("supporter", 1).apply();
                runOnUiThread(() -> { thanked = true; status = null; view.invalidate(); });
            }
        });
    }

    // ------------------------------------------------------------- view

    final class SupportView extends LauncherActivity.SceneView {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final RectF[] cards = { new RectF(), new RectF(), new RectF(), new RectF(), new RectF() };
        final RectF back = new RectF();
        int down = -1;

        SupportView(Context c) {
            super(c);
            p.setTypeface(LauncherActivity.font(c));
        }

        void text(Canvas c, String s, float x, float cy, float size, int col, Paint.Align a, float maxW) {
            p.setShader(null);
            p.setTextSize(size);
            if (maxW > 0 && p.measureText(s) > maxW) p.setTextSize(size * maxW / p.measureText(s));
            p.setTextAlign(a);
            Paint.FontMetrics fm = p.getFontMetrics();
            p.setColor(col);
            c.drawText(s, x, cy - (fm.ascent + fm.descent) / 2, p);
        }

        /** Word-wrapped paragraph, centred; returns the y after it. */
        float para(Canvas c, String s, float cx, float y, float maxW, float size, int col) {
            p.setTextSize(size);
            p.setTextAlign(Paint.Align.CENTER);
            p.setColor(col);
            Paint.FontMetrics fm = p.getFontMetrics();
            float lh = (fm.descent - fm.ascent) * 1.08f;
            StringBuilder line = new StringBuilder();
            for (String w : s.split(" ")) {
                String t = line.length() == 0 ? w : line + " " + w;
                if (p.measureText(t) > maxW && line.length() > 0) {
                    c.drawText(line.toString(), cx, y - fm.ascent, p);
                    y += lh;
                    line = new StringBuilder(w);
                } else line = new StringBuilder(t);
            }
            if (line.length() > 0) { c.drawText(line.toString(), cx, y - fm.ascent, p); y += lh; }
            return y;
        }

        @Override
        protected void onDraw(Canvas c) {
            int w = getWidth(), h = getHeight();
            drawScene(c, true);
            p.setShader(null);
            p.setColor(Color.argb(205, 8, 10, 26));
            c.drawRect(0, 0, w, h, p);

            float pw = Math.min(1040 * u, w - 40 * u);
            RectF pnl = new RectF((w - pw) / 2, 24 * u, (w + pw) / 2, h - 24 * u);
            p.setColor(Color.argb(110, 255, 90, 140));
            c.drawRoundRect(new RectF(pnl.left - 4 * u, pnl.top - 4 * u, pnl.right + 4 * u, pnl.bottom + 4 * u), 26 * u, 26 * u, p);
            p.setShader(new LinearGradient(0, pnl.top, 0, pnl.bottom, Color.argb(245, 32, 22, 58), Color.argb(245, 14, 12, 34), Shader.TileMode.CLAMP));
            c.drawRoundRect(pnl, 22 * u, 22 * u, p);
            p.setShader(null);
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(3 * u); p.setColor(Color.rgb(255, 120, 165));
            c.drawRoundRect(pnl, 22 * u, 22 * u, p);
            p.setStyle(Paint.Style.FILL);

            float cx = pnl.centerX();
            float y = pnl.top + 26 * u;
            Icons.draw(c, p, Icons.HEART, cx, y + 26 * u, 26 * u, Color.rgb(255, 96, 140));
            y += 64 * u;
            text(c, I18n.t(thanked ? "MUITO OBRIGADO!" : "APOIE UM DEV INDEPENDENTE"), cx, y + 18 * u, 36 * u, Color.WHITE, Paint.Align.CENTER, pw - 60 * u);
            y += 50 * u;
            String body = thanked
                ? I18n.t("Seu apoio chegou e faz toda a diferença. Agora você tem um coração ao lado do seu nome no placar. Bom jogo!")
                : I18n.t("Tunnel Clash é feito por um único desenvolvedor independente: sem anúncios, sem compras para vencer e sem coletar seus dados. Se você está curtindo, uma contribuição ajuda a manter o jogo vivo e a trazer novidades como multijogador e novos mapas.");
            y = para(c, body, cx, y, pw - 120 * u, 21 * u, Color.rgb(205, 210, 240));
            y += 22 * u;

            // tip cards
            float gap = 16 * u, cw = Math.min(200 * u, (pw - 60 * u - gap * (N - 1)) / N), ch = 170 * u;
            float x0 = cx - (cw * N + gap * (N - 1)) / 2;
            for (int i = 0; i < N; ++i) {
                RectF r = cards[i];
                r.set(x0 + i * (cw + gap), y, x0 + i * (cw + gap) + cw, y + ch);
                boolean big = i == FEATURED;
                int top = big ? Color.rgb(255, 120, 165) : Color.rgb(96, 72, 190);
                int bot = big ? Color.rgb(205, 42, 100) : Color.rgb(62, 44, 140);
                if (down == i) { top = Color.rgb(255, 160, 195); }
                p.setShader(new LinearGradient(0, r.top, 0, r.bottom, top, bot, Shader.TileMode.CLAMP));
                c.drawRoundRect(r, 18 * u, 18 * u, p);
                p.setShader(null);
                p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2.5f * u);
                p.setColor(big ? Color.rgb(255, 210, 225) : Color.rgb(170, 150, 255));
                c.drawRoundRect(r, 18 * u, 18 * u, p);
                p.setStyle(Paint.Style.FILL);
                int n = HEARTS[i];
                float hs = Math.min(13 * u, (cw - 24 * u) / (n * 2.4f)), hy = r.top + 38 * u;
                for (int k = 0; k < n; ++k)
                    Icons.draw(c, p, Icons.HEART, r.centerX() + (k - (n - 1) / 2f) * hs * 2.4f, hy, hs, Color.WHITE);
                text(c, I18n.t(TIER[i]), r.centerX(), r.top + 84 * u, 22 * u, Color.WHITE, Paint.Align.CENTER, cw - 24 * u);
                String price = details[i] != null && details[i].getOneTimePurchaseOfferDetails() != null
                    ? details[i].getOneTimePurchaseOfferDetails().getFormattedPrice() : "—";
                RectF pill = new RectF(r.left + 14 * u, r.bottom - 58 * u, r.right - 14 * u, r.bottom - 18 * u);
                p.setColor(Color.argb(90, 0, 0, 0));
                c.drawRoundRect(pill, 20 * u, 20 * u, p);
                text(c, price, pill.centerX(), pill.centerY(), 22 * u, Color.rgb(255, 225, 120), Paint.Align.CENTER, pill.width() - 16 * u);
            }
            y += ch + 18 * u;
            if (status != null)
                text(c, status, cx, y + 10 * u, 18 * u, Color.rgb(255, 200, 120), Paint.Align.CENTER, pw - 60 * u);
            else if (!thanked)
                text(c, I18n.t("Pagamento seguro pela Google Play. Não libera vantagens no jogo."), cx, y + 10 * u, 16 * u, Color.rgb(150, 160, 210), Paint.Align.CENTER, pw - 60 * u);

            back.set(cx - 150 * u, pnl.bottom - 74 * u, cx + 150 * u, pnl.bottom - 18 * u);
            LauncherActivity.Btn b = new LauncherActivity.Btn(0, prompt && !thanked ? "AGORA NÃO" : "VOLTAR", Icons.BACK, false, false, false);
            b.r.set(back);
            Icons.button(c, p, b, down == 9, u * 0.8f, p.getTypeface());
            postInvalidateOnAnimation();
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            float x = e.getX(), y = e.getY();
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    down = -1;
                    for (int i = 0; i < N; ++i) if (cards[i].contains(x, y)) down = i;
                    if (back.contains(x, y)) down = 9;
                    if (down >= 0) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                    return true;
                case MotionEvent.ACTION_UP: {
                    int d = down;
                    down = -1;
                    if (d == 9 && back.contains(x, y)) finish();
                    else if (d >= 0 && d < N && cards[d].contains(x, y)) buy(d);
                    return true;
                }
                case MotionEvent.ACTION_CANCEL:
                    down = -1;
                    return true;
            }
            return true;
        }
    }
}
