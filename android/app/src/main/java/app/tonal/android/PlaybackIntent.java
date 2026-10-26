package app.tonal.android;

/** Process-local transport intent. A failed reconnect never consumes the selection. */
final class PlaybackIntent {
    String selection="", track="";
    int position;
    volatile boolean desired;
    boolean local;
    private long recoveryStartedAt=-1;
    static final long RECOVERY_WINDOW_MS=120_000;
    long revision;
    void command(String command, boolean playing) {
        revision++;
        if(command.startsWith("editqueue|")){String[] parts=command.split("\\|",4);if(parts.length==4&&track.equals(parts[2]))selection=parts[3];return;}
        if(command.equals("pause")||command.equals("stop")||command.equals("play")||command.equals("toggle")||command.startsWith("restore|")||command.startsWith("requeue|")||command.startsWith("queue|")||command.startsWith("context|")||command.startsWith("spotify:"))recoveryStartedAt=-1;
        if(command.startsWith("restore|")||command.startsWith("requeue|")){String[] parts=command.split("\\|",4);if(parts.length==4){try{position=Math.max(0,Integer.parseInt(parts[1]));track=parts[2];selection=parts[3];desired=true;local=false;}catch(NumberFormatException ignored){}}return;}
        if(command.equals("stop")){selection="";track="";position=0;desired=false;return;}
        if(command.equals("pause")){desired=false;return;}
        if(command.equals("toggle")){desired=!playing;return;}
        if(command.equals("play")){desired=true;return;}
        if(command.startsWith("seek|")){try{position=Math.max(0,Integer.parseInt(command.substring(5)));}catch(NumberFormatException ignored){}return;}
        if(command.startsWith("queue|")||command.startsWith("context|")||command.startsWith("spotify:")) {
            selection=command;track="";position=0;desired=true;local=false;
        }
        if(command.equals("next")||command.equals("prev")){track="";position=0;}
    }
    void progress(String uri,int milliseconds,boolean playing) {
        if(!playing&&track.equals(uri)&&!uri.isEmpty())desired=false;
        if(!uri.isEmpty())track=uri;
        position=Math.max(0,milliseconds);
        // This callback uses the Player's Paused event, not sink suspension
        // while loading. A pause from another Connect controller also cancels.
    }
    String resumeSelection(){return !track.isEmpty()?track:selection;}
    String restoreCommand(){return "restore|"+position+"|"+track+"|"+selection;}
    boolean canRecover(long now){return desired&&!local&&!resumeSelection().isEmpty()&&(recoveryStartedAt<0||now-recoveryStartedAt<=RECOVERY_WINDOW_MS);}
    boolean beginRecovery(long now){if(!canRecover(now))return false;if(recoveryStartedAt<0)recoveryStartedAt=now;return true;}
    boolean audioStarted(long now){if(!desired||(recoveryStartedAt>=0&&now-recoveryStartedAt>RECOVERY_WINDOW_MS))return false;recoveryStartedAt=-1;return true;}
    boolean allowMediaPlay(boolean locked,boolean allowLocked){return (local||!resumeSelection().isEmpty())&&(!locked||allowLocked||desired);}
}
