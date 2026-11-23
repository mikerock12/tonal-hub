package app.tonal.android;

/** Pure Java policy regressions: no account, Android runtime or audio required. */
public final class PlaybackSafetyChecks {
    private static int checks;
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);checks++;}
    private static String source(String path){try{return new String(java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(path)),java.nio.charset.StandardCharsets.UTF_8);}catch(java.io.IOException e){throw new AssertionError("missing "+path);}}
    private static void transitionQueue() throws Exception {
        TransitionInputQueue queue=new TransitionInputQueue(2);float[] a={1,2},b={3,4};
        check(queue.offer(a,TransitionInputQueue.PCM,0,7),"queue accepts PCM");
        check(queue.offer(null,TransitionInputQueue.BOUNDARY,0,7),"queue preserves marker order");
        check(!queue.offer(b,TransitionInputQueue.PCM,0,7),"queue is bounded");
        TransitionInputQueue.Entry entry=queue.peek();check(entry.samples==a&&entry.epoch==7,"PCM is first");queue.release(entry);
        entry=queue.peek();check(entry.event==TransitionInputQueue.BOUNDARY,"marker follows prior PCM");queue.release(entry);
        check(queue.isEmpty(),"released slots are reusable");
        final int count=20000;TransitionInputQueue stress=new TransitionInputQueue(16);java.util.concurrent.atomic.AtomicReference<Throwable> failure=new java.util.concurrent.atomic.AtomicReference<>();
        Thread producer=new Thread(()->{try{for(int i=0;i<count;i++)stress.put(new float[]{i},TransitionInputQueue.PCM,0,9,()->false);}catch(Throwable error){failure.set(error);}});
        producer.start();
        for(int i=0;i<count;i++){TransitionInputQueue.Entry current;while((current=stress.peek())==null){if(failure.get()!=null)break;Thread.yield();}check(current.samples[0]==i&&current.epoch==9,"SPSC stress order "+i);stress.release(current);}
        producer.join(10000);check(!producer.isAlive()&&failure.get()==null&&stress.isEmpty(),"SPSC producer terminates without loss");
        TransitionInputQueue cancelled=new TransitionInputQueue(1);cancelled.offer(a,TransitionInputQueue.PCM,0,1);java.util.concurrent.atomic.AtomicBoolean stop=new java.util.concurrent.atomic.AtomicBoolean();java.util.concurrent.atomic.AtomicBoolean accepted=new java.util.concurrent.atomic.AtomicBoolean(true);java.util.concurrent.CountDownLatch waiting=new java.util.concurrent.CountDownLatch(1);
        Thread blocked=new Thread(()->{waiting.countDown();accepted.set(cancelled.put(b,TransitionInputQueue.PCM,0,1,stop::get));});blocked.start();waiting.await();
        long until=System.nanoTime()+2_000_000_000L;while(blocked.getState()!=Thread.State.WAITING&&System.nanoTime()<until)Thread.yield();stop.set(true);cancelled.wakeAll();blocked.join(2000);
        check(!blocked.isAlive()&&!accepted.get(),"full-queue wait is cancelled without accepting stale PCM");cancelled.release(cancelled.peek());
    }
    public static void main(String[] args) throws Exception {
        check(PositionSync.reconcile(12000,11500,true)==12000,"small stale progress cannot rewind playback");
        check(PositionSync.reconcile(12000,9000,true)==9000,"large external seeks remain possible");
        check(PositionSync.reconcile(12000,11500,false)==11500,"paused correction is respected");
        check(PositionSync.reconcile(12000,12500,true)==12500,"forward progress is accepted");
        for(String moved:new String[]{"next","prev","seek|1000","restore|0|a|b","queue|0|a","context|a","spotify:track:a"})check(PlaybackIntent.movesAudio(moved),"selection drops the previous tail: "+moved);
        for(String kept:new String[]{"pause","play","toggle","stop","requeue|0|a|b","editqueue|0|a|b"})check(!PlaybackIntent.movesAudio(kept),"audible decoder keeps its tail: "+kept);
        check(DiscoveryStyles.merge("",java.util.List.of("","grunge, noise rock","alternative rock")).equals("grunge,alternative rock,noise rock"),"unknown anchor uses styles from its selection");
        check(DiscoveryStyles.merge("Punk, power pop, garage rock",java.util.List.of("grunge")).equals("punk,power pop,grunge"),"anchor leads, neighbours add variety");
        check(DiscoveryStyles.merge("",java.util.List.of("","")).isEmpty(),"no known style means no invented query");
        check(DiscoveryStyles.merge("rock|rock;Rock",java.util.List.of()).equals("rock"),"styles are not repeated");
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
        check(!RepeatMode.loadsSelection("editqueue|0|spotify:track:a|queue|0|spotify:track:a"),"queue edit preserves repeat without issuing another command");
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
        transitionQueue();
        String panel=source(code+"MusicPanel.java"),audio=source(code+"AudioOutput.java"),transitions=source(code+"Transitions.java");
        check(panel.contains("if(closed||songPager!=pager)return")&&panel.contains("if(!closed&&songPager==pager)pager.showPage"),"cancelled touch cannot call a replaced SongPager");
        check(transitions.contains("PREROLL_MS+TransitionNative.LOAD_MARGIN_MS+500"),"transition capture leaves time for slow next-track loading");
        check(!audio.contains("mixLock.wait(50)")&&!audio.contains("Thread.sleep(2)"),"transition output has no timed monitor wait or sleep polling");
        check(audio.contains("TransitionInputQueue")&&audio.contains("transportDiscontinuity"),"bounded PCM handoff and typed discontinuity are wired");
        System.out.println("PLAYBACK_SAFETY_PASS "+checks+" checks");
    }
}
