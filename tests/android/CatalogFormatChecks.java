package app.tonal.android;

import java.util.Calendar;

public final class CatalogFormatChecks {
    private static int checks;
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);checks++;}
    private static void same(String expected,String actual){check(expected.equals(actual),"expected '"+expected+"' got '"+actual+"'");}
    public static void main(String[] args){
        same("",CatalogFormat.compact(-1));
        same("0",CatalogFormat.compact(0));
        same("999",CatalogFormat.compact(999));
        same("1K",CatalogFormat.compact(1000));
        same("2,2K",CatalogFormat.compact(2200));
        same("200,5K",CatalogFormat.compact(200500));
        same("4,5M",CatalogFormat.compact(4500000));
        same("4,8M",CatalogFormat.compact(4831164));
        same("11M",CatalogFormat.compact(10988356));
        same("27,4M",CatalogFormat.compact(27396836));
        same("1M",CatalogFormat.compact(999950));
        same("286,8M",CatalogFormat.compact(286786449));
        same("1,2B",CatalogFormat.compact(1234567890L));
        Calendar today=Calendar.getInstance();today.clear();today.set(2026,Calendar.NOVEMBER,26);
        same(" (aos 27 anos)",CatalogFormat.age("1967-02-20","1994-04-05",today));
        same(" (59 anos)",CatalogFormat.age("1967-02-20",null,today));
        same(" (58 anos)",CatalogFormat.age("1967-12-20","",today));
        same(" (cerca de 59 anos)",CatalogFormat.age("1967",null,today));
        same("",CatalogFormat.age("",null,today));
        same("",CatalogFormat.age("2030-01-01",null,today));
        same("1987",CatalogFormat.year("1987-04"));
        same("",CatalogFormat.year("87"));
        System.out.println("CATALOG_FORMAT_PASS: "+checks+" checks");
    }
}
