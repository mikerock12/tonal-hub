package app.tonal.android;

/**
 * Repeat state owned by the app. librespot clears repeat on every new selection
 * (queue, context, restore, requeue) and drops track repeat when the listener
 * skips ahead, as the official app does; the end of a repeated track does not
 * skip, so it plays again.
 */
final class RepeatMode {
    static final int OFF=0,QUEUE=1,TRACK=2;
    private RepeatMode(){}
    static int next(int mode){return (mode+1)%3;}
    static int sanitize(int mode){return mode==QUEUE||mode==TRACK?mode:OFF;}
    static boolean loadsSelection(String command){
        return command.startsWith("queue|")||command.startsWith("context|")||command.startsWith("restore|")||command.startsWith("requeue|")||command.startsWith("spotify:");
    }
    static int afterCommand(int mode,String command){return mode==TRACK&&command.equals("next")?QUEUE:mode;}
    static String[] engineCommands(int mode){return new String[]{mode!=OFF?"repeat:on":"repeat:off",mode==TRACK?"repeat-track:on":"repeat-track:off"};}
    static String icon(int mode){return mode==TRACK?"repeat_one":"repeat";}
    static String description(int mode){
        return mode==OFF?"Repetir a fila":mode==QUEUE?"Repetindo a fila: repetir só esta música":"Repetindo esta música: desligar repetição";
    }
    static String notice(int mode){return mode==OFF?"Repetição desligada":mode==QUEUE?"Repetindo a fila":"Repetindo esta música";}
}
