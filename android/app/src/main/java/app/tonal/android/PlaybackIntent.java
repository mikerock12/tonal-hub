package app.tonal.android;

/** Process-local transport intent. A failed reconnect never consumes the selection. */
final class PlaybackIntent {
    String selection="", track="";
    int position;
    boolean desired, local;
    long revision;
    void command(String command, boolean playing) {
        revision++;
        if(command.startsWith("restore|")){String[] parts=command.split("\\|",4);if(parts.length==4){try{position=Math.max(0,Integer.parseInt(parts[1]));track=parts[2];selection=parts[3];desired=true;local=false;}catch(NumberFormatException ignored){}}return;}
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
}
