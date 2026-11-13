package app.tonal.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.json.JSONObject;

/** Checks the public portal independently of Spotify, playback and the audio worker. */
final class AppUpdate {
    private static final String ENDPOINT = "https://softwares.smellsliketech.com.br/api/releases/tonal-android";
    private static final String ASSETS = "https://github.com/mikerock12/tonal-hub/releases/download/";
    private static final long MAX_APK = 256L * 1024 * 1024;

    record Release(String version, String notes, String url, String digest, long size) {}
    private final Activity activity;
    private Release release;
    private boolean busy;
    private boolean started;
    private String status = "";
    private AlertDialog dialog;
    private TextView details;

    AppUpdate(Activity activity) { this.activity = activity; }

    void onOpen() {
        if (BuildConfig.DEBUG || started) return;
        started = true;
        activity.getWindow().getDecorView().postDelayed(() -> check(true), 1800);
    }

    void about() {
        Ui ui = new Ui(activity);
        LinearLayout content = ui.column();
        ui.pad(content, 22, 14);
        content.addView(ui.text("Versão " + BuildConfig.VERSION_NAME + "\nProjeto original de Matheus Nunes (natheus). Tonal Hub mantido por Maicon Nunes (mikerock12). Licença MIT.", 14, Ui.TEXT));
        details = ui.text("", 14, Ui.TEXT);
        content.addView(details);
        android.widget.ScrollView scroll = new android.widget.ScrollView(activity);
        scroll.addView(content);
        dialog = new AlertDialog.Builder(activity).setTitle("Sobre o Tonal Hub").setView(scroll)
            .setNeutralButton("Verificar atualização", null).setPositiveButton("Baixar no portal", null)
            .setNegativeButton("Fechar", null).create();
        dialog.show();
        dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener(v -> check(false));
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> openPortal());
        render();
    }

    private void render() {
        if (dialog == null || !dialog.isShowing()) return;
        details.setText(status + (release == null ? "" : "\n\n" + release.notes()));
        dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setEnabled(!busy);
        View install = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
        install.setVisibility(release == null ? View.GONE : View.VISIBLE);
        install.setEnabled(!busy);
    }

    private boolean active() { return !activity.isFinishing() && !activity.isDestroyed(); }

    private void announce() {
        if (!active() || dialog != null && dialog.isShowing()) return;
        FrameLayout root = activity.findViewById(android.R.id.content);
        if (root == null) return;
        Ui ui = new Ui(activity);
        LinearLayout banner = ui.column();
        ui.pad(banner, 14, 10);
        ui.rounded(banner, Ui.BG, 12);
        banner.setElevation(ui.dp(12));
        TextView summary = ui.text("Versão " + release.version() + " disponível\n" + release.notes(), 14, Ui.TEXT);
        summary.setMaxLines(5);
        summary.setEllipsize(android.text.TextUtils.TruncateAt.END);
        banner.addView(summary);
        LinearLayout actions = ui.row();
        actions.addView(ui.action("Ver atualização", () -> { root.removeView(banner); about(); }));
        actions.addView(ui.action("Agora não", () -> root.removeView(banner)));
        banner.addView(actions);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        params.setMargins(ui.dp(12), ui.dp(56), ui.dp(12), 0);
        root.addView(banner, params);
    }

    private void check(boolean automatic) {
        if (busy || !active()) return;
        busy = true;
        status = "Consultando a versão no portal…";
        render();
        worker(() -> {
            Release next = null;
            String message;
            try {
                if (!Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a"))
                    throw new IOException("Atualização pelo portal disponível para Android ARM64.");
                next = parse(portalMetadata(), BuildConfig.VERSION_NAME);
                message = next == null ? "O Tonal Hub está atualizado." : "Versão " + next.version() + " disponível.";
            } catch (Exception e) {
                message = "Não foi possível verificar a atualização. Tente novamente.";
            }
            Release result = next;
            String text = message;
            activity.runOnUiThread(() -> {
                busy = false;
                if (!active()) return;
                release = result;
                status = text;
                render();
                if (automatic && result != null) announce();
            });
        });
    }

    static Release parse(JSONObject body, String installed) throws IOException {
        String tag = body.optString("version");
        if (!tag.matches("v[0-9]{1,6}\\.[0-9]{1,6}\\.[0-9]{1,6}"))
            throw new IOException("Versão inválida");
        String version = tag.substring(1);
        String[] remote = version.split("\\."), local = installed.split("-")[0].split("\\.");
        int comparison = 0;
        for (int i = 0; i < 3 && comparison == 0; i++)
            comparison = Integer.compare(Integer.parseInt(remote[i]), Integer.parseInt(local[i]));
        if (comparison <= 0) return null;
        String name = "Tonal-Android-" + version + "-arm64-v8a.apk";
        String expected = ASSETS + tag + "/" + name;
        String digest = body.optString("sha256");
        String notes = body.optString("notes").trim();
        long size = body.optLong("size");
        if (!expected.equals(body.optString("url")) || !name.equals(body.optString("name")) ||
            !digest.matches("[a-fA-F0-9]{64}") || notes.isEmpty() || notes.length() > 2000 || size < 1 || size > MAX_APK)
            throw new IOException("Metadados da atualização inválidos");
        return new Release(version, notes, expected, digest, size);
    }

    private void openPortal() {
        if (busy || release == null) return;
        try {
            activity.startActivity(new Intent(Intent.ACTION_VIEW,
                Uri.parse("https://softwares.smellsliketech.com.br/apps/tonal/#downloads")));
        } catch (android.content.ActivityNotFoundException e) {
            status = "Abra softwares.smellsliketech.com.br no navegador para baixar a atualização.";
            render();
        }
    }

    private static void worker(Runnable job) {
        new Thread(() -> { Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND); job.run(); }, "tonal-update").start();
    }

    private static JSONObject portalMetadata() throws Exception {
        HttpURLConnection conn = (HttpURLConnection)new URL(ENDPOINT).openConnection();
        conn.setConnectTimeout(12000);
        conn.setReadTimeout(18000);
        conn.setInstanceFollowRedirects(false);
        try {
            if (conn.getResponseCode() != 200) throw new IOException("Portal indisponível");
            try (InputStream in = conn.getInputStream()) {
                ByteArrayOutputStream body = new ByteArrayOutputStream();
                byte[] buffer = new byte[2048];
                for (int count; (count = in.read(buffer)) != -1;) {
                    if (body.size() + count > 16384) throw new IOException("Metadados muito grandes");
                    body.write(buffer, 0, count);
                }
                return new JSONObject(body.toString(StandardCharsets.UTF_8.name()));
            }
        } finally { conn.disconnect(); }
    }
}
