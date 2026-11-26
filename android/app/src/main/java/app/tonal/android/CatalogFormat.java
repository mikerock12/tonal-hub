package app.tonal.android;

import java.util.Calendar;
import java.util.Locale;

/** Pure formatting shared by catalog screens; no Android dependencies (host-testable). */
final class CatalogFormat {
    private CatalogFormat(){}
    /** 4,5M · 2,2K · 200,5K · 1,2B (pt-BR decimal comma); below 1000 the exact number. */
    static String compact(long value){
        if(value<0)return "";if(value<1000)return String.valueOf(value);
        String[] units={"K","M","B","T"};double v=value;int unit=-1;
        while(v>=1000&&unit<units.length-1){v/=1000;unit++;}
        double rounded=Math.round(v*10)/10.0;
        if(rounded>=1000&&unit<units.length-1){rounded=Math.round(rounded/100)/10.0;unit++;}
        String text=rounded==Math.floor(rounded)?String.valueOf((long)rounded):String.format(Locale.ROOT,"%.1f",rounded).replace('.',',');
        return text+units[unit];
    }
    static String year(String date){return date==null||date.length()<4?"":date.substring(0,4);}
    /** Completed years between a MusicBrainz partial date and death/today; empty when unknown. */
    static String age(String born,String died,Calendar now){
        try{int by=Integer.parseInt(born.substring(0,4));int ey=now.get(Calendar.YEAR),em=now.get(Calendar.MONTH)+1,ed=now.get(Calendar.DAY_OF_MONTH);
            if(died!=null&&died.length()>=4){ey=Integer.parseInt(died.substring(0,4));em=died.length()>=7?Integer.parseInt(died.substring(5,7)):12;ed=died.length()>=10?Integer.parseInt(died.substring(8,10)):31;}
            boolean exact=born.length()>=10;int years=ey-by;
            if(exact){int bm=Integer.parseInt(born.substring(5,7)),bd=Integer.parseInt(born.substring(8,10));if(em<bm||(em==bm&&ed<bd))years--;}
            if(years<0||years>130)return "";
            String text=(exact?"":"cerca de ")+years+" anos";return died!=null&&died.length()>=4?" (aos "+text+")":" ("+text+")";
        }catch(Exception e){return "";}
    }
}
