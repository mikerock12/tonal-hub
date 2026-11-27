package app.tonal.android;

import java.util.Locale;

/** Rainbow color rail of the lyric style dialog: 1001 positions (0..1000) over 16 color stops, from white
 *  through the spectrum to black. Same table and arithmetic as qml/companion/LyricColors.js. */
final class LyricRail {
    static final int MAX=1000;
    static final int[] STOPS={0xffffff,0xff0000,0xff4000,0xff8000,0xffbf00,0xffff00,0x80ff00,0x00ff00,0x00ff80,0x00ffff,0x0080ff,0x0000ff,0x8000ff,0xff00ff,0xff0080,0x000000};
    private LyricRail(){}
    static int color(int position){
        int p=Math.max(0,Math.min(MAX,position)),segments=STOPS.length-1,scaled=p*segments;int segment=Math.min(segments-1,scaled/MAX);double t=(scaled-segment*MAX)/(double)MAX;
        int a=STOPS[segment],b=STOPS[segment+1];
        return 0xff000000|(channel(a,b,16,t)<<16)|(channel(a,b,8,t)<<8)|channel(a,b,0,t);
    }
    private static int channel(int a,int b,int shift,double t){int x=(a>>shift)&255,y=(b>>shift)&255;return (int)Math.round(x+(y-x)*t);}
    static String hex(int color){return String.format(Locale.ROOT,"#%06X",color&0xffffff);}
    /** Closest rail position for a stored color (e.g. a color chosen before 0.18.2). */
    static int nearest(int color){
        int best=0;long distance=Long.MAX_VALUE;
        for(int p=0;p<=MAX;p++){int c=color(p);long dr=((c>>16)&255)-((color>>16)&255),dg=((c>>8)&255)-((color>>8)&255),db=(c&255)-(color&255);long value=dr*dr+dg*dg+db*db;if(value<distance){distance=value;best=p;}}
        return best;
    }
}
