package app.tonal.android;

/** Host check for the lyric color rail (no Android runtime). The same values are pinned in tests/lyrics/tst_inline.qml. */
public final class LyricColorChecks {
    private static int checks;
    private static void expect(boolean ok,String what){if(!ok)throw new AssertionError(what);checks++;}
    public static void main(String[] args){
        expect(LyricRail.STOPS.length==16,"16 color stops");
        String[][] pinned={{"0","#FFFFFF"},{"50","#FF4040"},{"200","#FF8000"},{"400","#80FF00"},{"600","#00FFFF"},{"800","#8000FF"},{"1000","#000000"}};
        for(String[] p:pinned){String got=LyricRail.hex(LyricRail.color(Integer.parseInt(p[0])));expect(got.equals(p[1]),"rail "+p[0]+" = "+got+", expected "+p[1]);}
        expect(LyricRail.color(-5)==LyricRail.color(0)&&LyricRail.color(5000)==LyricRail.color(1000),"rail bounds");
        java.util.HashSet<Integer> distinct=new java.util.HashSet<>();int unchanged=0;
        for(int p=0;p<=LyricRail.MAX;p++){distinct.add(LyricRail.color(p));if(p>0&&LyricRail.color(p)==LyricRail.color(p-1))unchanged++;}
        expect(distinct.size()>=980&&unchanged<=20,"almost every +/- step changes the color: "+distinct.size()+" distinct, "+unchanged+" repeated");
        for(int p:new int[]{0,1,250,555,999,1000})expect(LyricRail.nearest(LyricRail.color(p))==p,"nearest round-trip "+p);
        expect(LyricRail.nearest(0xfff5f5f5)<5,"old default text color maps next to white");
        System.out.println("LYRIC_COLOR_PASS "+checks+" checks, "+distinct.size()+" distinct colors on 1001 positions");
    }
}
