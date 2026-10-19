package app.tonal.android;

import android.app.Instrumentation;
import android.content.Intent;
import android.os.SystemClock;
import org.json.*;
import java.io.File;
import java.lang.reflect.*;
import java.util.*;

/** Device regression tests; excluded from the release APK. No account, track or credential needed. */
final class CoreTests {
    private final JSONArray checks=new JSONArray();
    private void require(boolean pass,String name)throws Exception{if(!pass)throw new AssertionError(name);checks.put(name);}
    JSONObject run(Instrumentation instrumentation)throws Exception{
        return run(instrumentation,false);
    }
    JSONObject run(Instrumentation instrumentation,boolean playbackOnly)throws Exception{
        if(!playbackOnly){
        final JSONArray[] companion={null};instrumentation.runOnMainSync(()->{try{companion[0]=CompanionDeviceTests.run(instrumentation.getTargetContext());}catch(Exception e){throw new RuntimeException(e);}});
        for(Object value:MusicStore.values(companion[0]))checks.put(value);
        }
        PlaybackIntent state=new PlaybackIntent();state.command("queue|1|spotify:track:first,spotify:track:second",false);state.progress("spotify:track:second",42600,true);
        require(state.resumeSelection().equals("spotify:track:second")&&state.position==42600&&state.desired,"resume_uses_actual_pcm_track_and_position");
        require(state.restoreCommand().equals("restore|42600|spotify:track:second|queue|1|spotify:track:first,spotify:track:second"),"restore_preserves_queue_and_atomic_seek");
        String saved=state.resumeSelection();state.command("pause",false);require(!state.desired&&state.resumeSelection().equals(saved),"pause_cancels_resume_without_losing_selection");
        state.command("spotify:track:new",false);require(state.desired&&state.position==0&&state.resumeSelection().equals("spotify:track:new"),"new_selection_replaces_old_resume");
        state.command("stop",false);require(!state.desired&&state.resumeSelection().isEmpty(),"stop_cancels_all_recovery_intent");
        IndieCatalog.Item exact=new IndieCatalog.Item("MusicBrainz","real","artist","Véruca Salt");exact.apiScore=100;
        IndieCatalog.Item unrelated=new IndieCatalog.Item("Audius","other","artist","Salt beats remix");
        IndieCatalog.Item cover=new IndieCatalog.Item("Audius","cover","track","Veruca Salt remix");cover.artist="Other producer";cover.playable=true;
        ArrayList<IndieCatalog.Item> ranked=new ArrayList<>(List.of(cover,unrelated,exact));IndieCatalog.sort(ranked,"Veruca Salt");require(ranked.get(0)==exact,"exact_artist_ahead_of_remixes_and_unrelated_profiles");
        IndieCatalog.Item us=new IndieCatalog.Item("MusicBrainz","US","artist","Nirvana"),uk=new IndieCatalog.Item("MusicBrainz","UK","artist","Nirvana");us.apiScore=100;uk.apiScore=82;ranked=new ArrayList<>(List.of(uk,us));IndieCatalog.sort(ranked,"Nirvana");require(ranked.get(0)==us,"provider_score_breaks_homonym_ties");
        require(!exact.playable,"musicbrainz_is_metadata_only");require(IndieCatalog.normalize("Alice in Chains").equals("alice in chains"),"unicode_and_word_normalization");
        boolean blocked=false;try{WebRequest.url("http://example.org/audio.mp3");}catch(Exception expected){blocked=true;}require(blocked,"reject_cleartext_streams");blocked=false;try{WebRequest.url("https://user:password@example.org/audio");}catch(Exception expected){blocked=true;}require(blocked,"reject_url_userinfo");
        String safe=Diagnostics.sanitize("Bearer private-token-value access_token=secret-pass https://host/path?password=secret-pass");require(!safe.contains("private-token-value")&&!safe.contains("secret-pass"),"diagnostics_redact_urls_and_credentials");
        // Real service + JNI: no credentials in a fresh benchmark install. Verify
        // failed native sessions stop retrying and explicit pause stays cancelled.
        File credentials=new File(instrumentation.getTargetContext().getFilesDir(),"recovery-test-no-account/credentials.json");
        if(credentials.exists())throw new IllegalStateException("Use a fresh benchmark install for recovery tests");
        android.app.Activity activity=instrumentation.startActivitySync(new Intent(instrumentation.getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        Field pending=MainActivity.class.getDeclaredField("pendingPlayback");pending.setAccessible(true);
        Field foreground=MainActivity.class.getDeclaredField("foreground");foreground.setAccessible(true);
        boolean[] unexpected={false};
        instrumentation.runOnMainSync(()->{try{pending.set(activity,(Runnable)()->unexpected[0]=true);activity.moveTaskToBack(true);}catch(Exception e){throw new RuntimeException(e);}});
        long backgroundDeadline=SystemClock.elapsedRealtime()+10000;
        while(foreground.getBoolean(activity)&&SystemClock.elapsedRealtime()<backgroundDeadline)Thread.sleep(50);
        require(!foreground.getBoolean(activity)&&pending.get(activity)==null,"leaving_activity_cancels_pending_selection");
        NativeBridge.onStatus("Pronto • readiness fixture");Thread.sleep(150);
        require(!unexpected[0],"late_ready_event_cannot_start_background_selection");
        instrumentation.runOnMainSync(()->activity.startActivity(new Intent(activity,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)));
        Thread.sleep(300);
        instrumentation.runOnMainSync(()->{NativeBridge.command("stop");});
        NativeBridge.spotifyEngine=true;
        Thread.sleep(100); // allow the intentional 30 ms output fade to finish
        long idleFrames=NativeBridge.audio.frames.get();
        NativeBridge.onAudioStart(44100);NativeBridge.onPcm(new float[2048]);
        require(!NativeBridge.playing&&!NativeBridge.audio.isPlaying()&&NativeBridge.audio.frames.get()==idleFrames,"unsolicited_native_start_and_pcm_blocked_without_play_intent");
        instrumentation.runOnMainSync(()->{NativeBridge.audio.initialize(instrumentation.getTargetContext());synchronized(NativeBridge.intent){NativeBridge.intent.command("spotify:track:pcm-fixture",false);}NativeBridge.onAudioStart(44100);});
        NativeBridge.onPcm(new float[17640]);
        require(NativeBridge.playing&&NativeBridge.audio.frames.get()>idleFrames,"authorized_output_still_accepts_pcm");
        instrumentation.runOnMainSync(()->NativeBridge.command("pause"));Thread.sleep(100);
        long held=NativeBridge.audio.frames.get();NativeBridge.onAudioStart(44100);NativeBridge.onPcm(new float[2048]);
        require(!NativeBridge.playing&&!NativeBridge.audio.isPlaying()&&NativeBridge.audio.frames.get()==held,"pause_blocks_late_pcm_after_real_output");
        synchronized(NativeBridge.intent){NativeBridge.intent.command("spotify:track:recovery-fixture",false);}
        instrumentation.runOnMainSync(()->{instrumentation.getTargetContext().startForegroundService(new Intent(instrumentation.getTargetContext(),PlaybackService.class).setAction("connect").putExtra("value","").putExtra("recoveryTest",true));});
        long deadline=SystemClock.elapsedRealtime()+25000;while(SystemClock.elapsedRealtime()<deadline&&!NativeBridge.status.startsWith("Recuperação interrompida"))Thread.sleep(100);
        require(NativeBridge.status.startsWith("Recuperação interrompida"),"real_jni_connection_failures_hit_bounded_recovery_budget");
        long epoch=NativeBridge.generation;Thread.sleep(2000);require(NativeBridge.generation==epoch,"no_unbounded_restart_loop_after_budget");
        instrumentation.runOnMainSync(()->{NativeBridge.command("pause");});require(!NativeBridge.intent.desired,"explicit_pause_cancels_service_resume");
        Thread.sleep(100);
        long pausedFrames=NativeBridge.audio.frames.get();NativeBridge.onAudioStart(44100);NativeBridge.onAudioResume();NativeBridge.onPcm(new float[2048]);
        require(!NativeBridge.playing&&!NativeBridge.audio.isPlaying()&&NativeBridge.audio.frames.get()==pausedFrames,"late_native_callbacks_cannot_override_explicit_pause");
        instrumentation.runOnMainSync(()->instrumentation.getTargetContext().stopService(new Intent(instrumentation.getTargetContext(),PlaybackService.class)));
        return new JSONObject().put("passed",true).put("checks",checks);
    }
}
