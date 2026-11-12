package app.tonal.android;

import android.os.SystemClock;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import org.json.JSONObject;

/** Extends Spotify's short queue window with the verified tail of its context. */
final class ContextQueue {
    record Snapshot(ArrayList<JSONObject> tracks, boolean partial) {}

    private String cachedContext = "";
    private long cachedAt;
    private ArrayList<JSONObject> cachedTracks;

    void clear() {
        cachedContext = "";
        cachedTracks = null;
    }

    void resolve(SpotifyClient api, String context, boolean shuffled, ArrayList<JSONObject> observed,
                 BooleanSupplier valid, Consumer<Snapshot> done) {
        boolean album = context.startsWith("spotify:album:");
        boolean supported = album || context.startsWith("spotify:playlist:");
        if (!supported || shuffled) {
            done.accept(new Snapshot(observed, supported));
            return;
        }
        if (cachedTracks != null && context.equals(cachedContext) && SystemClock.elapsedRealtime()-cachedAt < 60_000) {
            done.accept(extend(observed, cachedTracks, true));
            return;
        }
        String id = context.substring(context.lastIndexOf(':')+1);
        String path = album ? "albums/"+id : "playlists/"+id+"/items?limit=50";
        page(api, path, album, null, new ArrayList<>(), 0, valid, (rows, complete) -> {
            if (complete && valid.getAsBoolean()) {
                cachedTracks = rows;
                cachedContext = context;
                cachedAt = SystemClock.elapsedRealtime();
            }
            done.accept(extend(observed, rows, complete));
        });
    }

    private interface PageResult { void accept(ArrayList<JSONObject> rows, boolean complete); }

    private void page(SpotifyClient api, String path, boolean album, JSONObject albumInfo,
                      ArrayList<JSONObject> rows, int page, BooleanSupplier valid, PageResult done) {
        api.get(path, response -> {
            if (!valid.getAsBoolean()) { done.accept(new ArrayList<>(), false); return; }
            JSONObject group = response;
            JSONObject metadata = albumInfo;
            if (album && response.optJSONObject("tracks") != null) {
                group = response.optJSONObject("tracks");
                metadata = new JSONObject();
                for (String key : new String[]{"id","name","images","release_date"})
                    if (response.has(key)) MusicStore.put(metadata, key, response.opt(key));
            }
            for (Object value : MusicStore.values(MusicStore.array(group, "items"))) {
                if (!(value instanceof JSONObject item)) continue;
                JSONObject track = album ? item : item.optJSONObject("item");
                if (!album && track == null) track = item.optJSONObject("track");
                if (track == null || !track.optString("uri").startsWith("spotify:track:") ||
                    track.optBoolean("is_local") || !track.optBoolean("is_playable", true)) continue;
                if (!track.has("album") && metadata != null) MusicStore.put(track,"album",metadata);
                if (rows.size() < 2000) rows.add(track);
            }
            String next = group.isNull("next") ? "" : group.optString("next");
            if (!next.isEmpty() && !next.equals(path) && page < 39 && rows.size() < 2000)
                page(api,next,album,metadata,rows,page+1,valid,done);
            else done.accept(rows,next.isEmpty());
        },error -> done.accept(new ArrayList<>(),false));
    }

    static Snapshot extend(ArrayList<JSONObject> observed, List<JSONObject> catalog, boolean complete) {
        if (observed.isEmpty()) return new Snapshot(observed,true);
        String current = observed.get(0).optString("uri");
        int best = -1, suffix = -1, upcoming = observed.size()-1;
        for (int i = 0; i < catalog.size(); i++) {
            if (!current.equals(catalog.get(i).optString("uri"))) continue;
            for (int count = Math.min(upcoming,catalog.size()-i-1); count >= 0; count--) {
                if (count == 0 && upcoming > 0) break;
                boolean matches = true;
                for (int j = 0; j < count && matches; j++)
                    matches = observed.get(observed.size()-count+j).optString("uri").equals(catalog.get(i+1+j).optString("uri"));
                if (!matches) continue;
                if (count > best) {
                    best = count;
                    suffix = i+1+count;
                } else if (count == best) suffix = -1;
                break;
            }
        }
        if (suffix < 0 || (best < upcoming && best < 2)) return new Snapshot(observed,true);
        int total = observed.size()+catalog.size()-suffix;
        ArrayList<JSONObject> tracks = new ArrayList<>(observed.subList(0,Math.min(500,observed.size())));
        tracks.addAll(catalog.subList(suffix,Math.min(catalog.size(),suffix+500-tracks.size())));
        return new Snapshot(tracks,!complete || total > 500);
    }
}
