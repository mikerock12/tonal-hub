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
    interface Reader { void get(String path, Consumer<JSONObject> success, Consumer<String> failure); }

    private String cachedContext = "";
    private long cachedAt;
    private ArrayList<JSONObject> cachedTracks;

    void clear() {
        cachedContext = "";
        cachedTracks = null;
    }

    static String contextUri(JSONObject state) {
        JSONObject context = state.optJSONObject("context");
        return context == null ? "" : context.optString("uri", "");
    }

    private static boolean collection(String context) {
        return context.matches("spotify:(album|playlist):[A-Za-z0-9]{22}");
    }

    static String selectionUri(String command) {
        String[] parts = command.split("\\|", 3);
        String uri = parts.length == 3 && parts[0].equals("context") ? parts[2] : command;
        return collection(uri) ? uri : "";
    }

    void read(Reader api, JSONObject current, String anchor, String context, String selected,
              boolean shuffled, BooleanSupplier valid, Consumer<Snapshot> done, Consumer<String> error) {
        String changed = "A música ou o dispositivo mudou. Abra a fila novamente.";
        Consumer<Snapshot> deliver = snapshot -> {
            if (valid.getAsBoolean()) done.accept(snapshot);
            else error.accept(changed);
        };
        api.get("me/player/queue", result -> {
            JSONObject playing = result.optJSONObject("currently_playing");
            if (playing == null) playing = current;
            if (!valid.getAsBoolean() || playing == null || !anchor.equals(playing.optString("uri"))) {
                error.accept(changed);
                return;
            }
            ArrayList<JSONObject> observed = new ArrayList<>();
            observed.add(playing);
            for (Object value : MusicStore.values(MusicStore.array(result, "queue")))
                if (value instanceof JSONObject track) observed.add(track);
            if (!context.isEmpty()) {
                resolve(api, context, shuffled, observed, valid, deliver);
                return;
            }
            // The periodic player poll may have no context while Connect keeps playing.
            // Recover the context on demand, using the last selection only when its order matches.
            Consumer<Boolean> fromSelection = shuffle -> {
                if (!valid.getAsBoolean()) { error.accept(changed); return; }
                if (!collection(selected) || observed.size() < 3)
                    deliver.accept(new Snapshot(observed, observed.size() >= 21 || collection(selected)));
                else resolve(api, selected, shuffle, observed, valid, deliver);
            };
            api.get("me/player", state -> {
                if (!valid.getAsBoolean()) { error.accept(changed); return; }
                JSONObject item = state.optJSONObject("item");
                if (item != null && !anchor.equals(item.optString("uri"))) {
                    error.accept(changed);
                    return;
                }
                String live = contextUri(state);
                boolean liveShuffle = state.optBoolean("shuffle_state", shuffled);
                if (live.isEmpty()) fromSelection.accept(liveShuffle);
                else resolve(api, live, liveShuffle, observed, valid, deliver);
            }, failure -> fromSelection.accept(shuffled));
        }, error);
    }

    private void resolve(Reader api, String context, boolean shuffled, ArrayList<JSONObject> observed,
                 BooleanSupplier valid, Consumer<Snapshot> done) {
        boolean album = context.startsWith("spotify:album:");
        boolean supported = collection(context);
        if (!supported || shuffled) {
            done.accept(new Snapshot(observed, supported || observed.size() >= 21));
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

    private void page(Reader api, String path, boolean album, JSONObject albumInfo,
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
        int best = -1, suffix = -1, upcoming = observed.size()-1;
        for (int i = 0; i < catalog.size(); i++) {
            if (!sameTrack(observed.get(0), catalog.get(i))) continue;
            for (int count = Math.min(upcoming,catalog.size()-i-1); count >= 0; count--) {
                if (count == 0 && upcoming > 0) break;
                boolean matches = true;
                for (int j = 0; j < count && matches; j++)
                    matches = sameTrack(observed.get(observed.size()-count+j), catalog.get(i+1+j));
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

    private static boolean sameTrack(JSONObject first, JSONObject second) {
        String uri = first.optString("uri");
        String other = second.optString("uri");
        if (uri.isEmpty() || other.isEmpty()) return false;
        if (uri.equals(other)) return true;
        JSONObject linked = first.optJSONObject("linked_from");
        JSONObject otherLinked = second.optJSONObject("linked_from");
        return (linked != null && other.equals(linked.optString("uri"))) ||
               (otherLinked != null && uri.equals(otherLinked.optString("uri"))) ||
               (linked != null && otherLinked != null && !linked.optString("uri").isEmpty() &&
                linked.optString("uri").equals(otherLinked.optString("uri")));
    }
}
