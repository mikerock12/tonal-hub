package app.tonal.android;

/** Ignore small backwards corrections while playing; deliberate seeks anchor first. */
final class PositionSync {
    private PositionSync() {}
    static int reconcile(int displayed, int reported, boolean playing) {
        if (playing && reported < displayed && (long) displayed - reported <= 1500) return displayed;
        return Math.max(0, reported);
    }
}
