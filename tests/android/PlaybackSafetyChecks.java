package app.tonal.android;

/** Pure Java policy regressions: no account, Android runtime or audio required. */
public final class PlaybackSafetyChecks {
    private static int checks;
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);checks++;}
    public static void main(String[] args){
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
        System.out.println("PLAYBACK_SAFETY_PASS "+checks+" checks");
    }
}
