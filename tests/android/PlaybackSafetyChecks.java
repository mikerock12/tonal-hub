package app.tonal.android;

/** Pure Java policy regressions: no account, Android runtime or audio required. */
public final class PlaybackSafetyChecks {
    private static int checks;
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);checks++;}
    private static String source(String path){try{return new String(java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(path)),java.nio.charset.StandardCharsets.UTF_8);}catch(java.io.IOException e){throw new AssertionError("missing "+path);}}
    public static void main(String[] args){
        check(PositionSync.reconcile(12000,11500,true)==12000,"small stale progress cannot rewind playback");
        check(PositionSync.reconcile(12000,9000,true)==9000,"large external seeks remain possible");
        check(PositionSync.reconcile(12000,11500,false)==11500,"paused correction is respected");
        check(PositionSync.reconcile(12000,12500,true)==12500,"forward progress is accepted");
        PlaybackIntent state=new PlaybackIntent();
        check(!state.desired&&!state.canRecover(1000),"cold launch must be silent");
        check(!state.allowMediaPlay(false,true),"media play without a selected session is rejected");
        state.command("queue|0|spotify:track:first,spotify:track:second",false);
        state.progress("spotify:track:first",5000,true);
        check(state.beginRecovery(1000),"interrupted user playback may recover");
        check(state.canRecover(120999),"short interruption preserves playback");
        check(!state.canRecover(121001),"late reconnection cannot restart an old session");
        check(!state.audioStarted(121001),"native start after expired recovery is rejected");
        long revision=state.revision;state.command("pause",true);
        check(!state.desired&&state.revision!=revision&&!state.beginRecovery(2000),"pause invalidates pending recovery");
        state.progress("spotify:track:first",6000,true);
        check(!state.desired,"late native playing event must not rearm playback");
        check(!state.allowMediaPlay(true,false),"unsolicited media play while locked is blocked by default");
        check(state.allowMediaPlay(true,true),"locked media resume remains an explicit preference");
        check(state.allowMediaPlay(false,false),"external controls may resume an unlocked selected session");
        state.command("play",false);
        check(state.desired&&state.beginRecovery(500000),"new explicit play opens a fresh recovery window");
        check(state.audioStarted(500100)&&state.beginRecovery(900000),"successful output resets the interruption window");
        state.command("stop",true);state.progress("spotify:track:stale",7000,true);
        check(!state.desired&&!state.canRecover(500001),"stale progress after stop cannot restart audio");
        state.local=true;state.command("play",false);
        check(!state.canRecover(500002),"Spotify recovery never takes over a local file");
        state.command("pause",true);
        check(state.allowMediaPlay(false,false)&&!state.allowMediaPlay(true,false),"local files retain unlocked resume controls and locked protection");
        // Applying a reordered queue keeps the audible track: the intent records the
        // new selection for recovery, and a recovery reloads it with a full restore.
        PlaybackIntent mix=new PlaybackIntent();mix.command("queue|0|spotify:track:a,spotify:track:b",false);mix.progress("spotify:track:a",42000,true);
        mix.command("requeue|42250|spotify:track:a|queue|0|spotify:track:a,spotify:track:x,spotify:track:b",true);
        check(mix.desired&&mix.position==42250&&mix.track.equals("spotify:track:a")&&mix.selection.equals("queue|0|spotify:track:a,spotify:track:x,spotify:track:b"),"requeue keeps the current track and records the new order");
        check(mix.restoreCommand().startsWith("restore|42250|spotify:track:a|queue|0|"),"recovery after requeue reloads the new order with restore");
        check(mix.beginRecovery(3000),"requeue is an explicit selection that may recover");
        mix.command("seek|90000",true);
        check(mix.position==90000&&mix.desired,"seek updates the recovery position without changing intent");
        mix.command("pause",true);mix.command("requeue|1000|spotify:track:a|queue|0|spotify:track:a",false);
        check(mix.desired,"applying a queue with play is an explicit play request");
        mix.command("pause",true);int held=mix.position;
        mix.command("editqueue|90000|spotify:track:a|queue|0|spotify:track:a,spotify:track:new",false);
        check(!mix.desired&&mix.position==held&&mix.selection.endsWith("spotify:track:new"),"queue edit preserves pause and live position");
        mix.command("editqueue|0|spotify:track:stale|queue|0|stale",false);
        check(!mix.desired&&!mix.selection.endsWith("stale"),"late edit cannot replace a different track");
        check(RepeatMode.loadsSelection("editqueue|0|spotify:track:a|queue|0|spotify:track:a"),"queue edit reapplies repeat");
        // Repeat: off -> queue -> track -> off; reapplied after every selection
        // load; skipping ahead leaves track repeat, the end of a track does not.
        check(RepeatMode.next(RepeatMode.OFF)==RepeatMode.QUEUE&&RepeatMode.next(RepeatMode.QUEUE)==RepeatMode.TRACK&&RepeatMode.next(RepeatMode.TRACK)==RepeatMode.OFF,"repeat cycles through three modes");
        check(String.join(",",RepeatMode.engineCommands(RepeatMode.TRACK)).equals("repeat:on,repeat-track:on"),"track repeat enables both engine flags");
        check(String.join(",",RepeatMode.engineCommands(RepeatMode.QUEUE)).equals("repeat:on,repeat-track:off"),"queue repeat clears track repeat");
        check(String.join(",",RepeatMode.engineCommands(RepeatMode.OFF)).equals("repeat:off,repeat-track:off"),"off clears both flags");
        for(String load:new String[]{"queue|0|spotify:track:a","context|0|spotify:album:x","restore|1|spotify:track:a|queue|0|spotify:track:a","requeue|1|spotify:track:a|queue|0|spotify:track:a","spotify:track:a"})
            check(RepeatMode.loadsSelection(load),"selection load reapplies repeat: "+load);
        for(String other:new String[]{"next","prev","pause","toggle","seek|1000","repeat:on","shuffle:off"})
            check(!RepeatMode.loadsSelection(other),"transport command keeps engine repeat: "+other);
        check(RepeatMode.afterCommand(RepeatMode.TRACK,"next")==RepeatMode.QUEUE,"skipping ahead leaves track repeat like the official app");
        check(RepeatMode.afterCommand(RepeatMode.TRACK,"prev")==RepeatMode.TRACK&&RepeatMode.afterCommand(RepeatMode.QUEUE,"next")==RepeatMode.QUEUE,"other commands keep the chosen repeat");
        check(RepeatMode.sanitize(7)==RepeatMode.OFF&&RepeatMode.icon(RepeatMode.TRACK).equals("repeat_one"),"saved values are sanitized and track repeat has its own icon");
        // Media keys: Tonalify is the remembered target, so a headset or car does not wake the Spotify app.
        String code="android/app/src/main/java/app/tonal/android/",manifest=source("android/app/src/main/AndroidManifest.xml"),receiver=source(code+"MediaButtonReceiver.java");
        check(manifest.contains("<receiver android:name=\".MediaButtonReceiver\"")&&manifest.contains("android.intent.action.MEDIA_BUTTON"),"media button receiver is declared");
        check(!receiver.contains("NativeBridge.command")&&!receiver.contains("startForegroundService")&&!receiver.contains("PlaybackService.start")&&!receiver.contains("startActivity"),"a media key with the player closed never starts playback or opens an app");
        for(String service:new String[]{"PlaybackService.java","IndiePlaybackService.java"})check(source(code+service).contains("MediaButtonReceiver.attach(media,this);media.setActive(true);"),"session registers the receiver before activation: "+service);
        System.out.println("PLAYBACK_SAFETY_PASS "+checks+" checks");
    }
}
