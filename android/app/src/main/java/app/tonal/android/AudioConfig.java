package app.tonal.android;

import android.content.SharedPreferences;

/** Settings stored per output, with conservative defaults and migration from the old mobile EQ. */
final class AudioConfig {
    static final String[] PRESETS={"Flat","Rock","Pop","Clássica","Eletrônica","Vocal"};
    static final float[][] CURVES={new float[10],{0,2,1,-1,-1,0,1,2,1,0},{-1,1,2,1,0,1,2,1,0,-1},{0,0,0,0,-1,-1,0,1,1,0},{2,2,1,0,-1,0,1,2,1,0},{-2,-1,-1,0,1,2,2,1,0,-1}};
    boolean enabled=true,eq=false,compressor=false,normalization=true;
    float[] gains=new float[10];float bass=0,target=-16,ceiling=-1;
    int spatial=0,profile=0,buffer=0,preset=0;
    static AudioConfig load(SharedPreferences p,String key){
        AudioConfig c=new AudioConfig();String k="audio."+key+".";
        if(!p.contains(k+"version")){if(key.equals("default")){for(int i=0;i<10;i++){c.gains[i]=p.getFloat("eq"+i,0);if(c.gains[i]!=0)c.eq=true;}c.bass=p.getFloat("bass",0);c.compressor=p.getBoolean("compressor",false);}return c;}
        c.enabled=p.getBoolean(k+"enabled",true);c.eq=p.getBoolean(k+"eq",false);c.compressor=p.getBoolean(k+"compressor",false);c.normalization=p.getBoolean(k+"normalization",true);
        for(int i=0;i<10;i++)c.gains[i]=clamp(p.getFloat(k+"band"+i,0),-12,12);
        c.bass=clamp(p.getFloat(k+"bass",0),0,12);c.target=clamp(p.getFloat(k+"target",-16),-24,-10);c.ceiling=clamp(p.getFloat(k+"ceiling",-1),-6,-.1f);
        c.spatial=Math.max(0,Math.min(2,p.getInt(k+"spatial",0)));c.profile=Math.max(0,Math.min(3,p.getInt(k+"profile",0)));c.buffer=Math.max(0,Math.min(2,p.getInt(k+"buffer",0)));c.preset=Math.max(0,Math.min(5,p.getInt(k+"preset",0)));return c;
    }
    static float clamp(float v,float min,float max){return Float.isFinite(v)?Math.max(min,Math.min(max,v)):min;}
    void save(SharedPreferences p,String key){String k="audio."+key+".";SharedPreferences.Editor e=p.edit().putInt(k+"version",1).putBoolean(k+"enabled",enabled).putBoolean(k+"eq",eq).putBoolean(k+"compressor",compressor).putBoolean(k+"normalization",normalization).putFloat(k+"bass",bass).putFloat(k+"target",target).putFloat(k+"ceiling",ceiling).putInt(k+"spatial",spatial).putInt(k+"profile",profile).putInt(k+"buffer",buffer).putInt(k+"preset",preset);for(int i=0;i<10;i++)e.putFloat(k+"band"+i,gains[i]);e.apply();}
    static int bufferMillis(int mode,boolean bluetooth){return mode==1?(bluetooth?140:80):mode==2?(bluetooth?40:20):(bluetooth?60:40);}
    static int growBuffer(int current,int rate,int capacity){return Math.min(capacity,Math.min(rate/4,current+Math.max(1,rate/50)));}
    void preset(int index){preset=index;gains=CURVES[index].clone();eq=index!=0;bass=index==1?2:index==4?3:0;}
    Dsp.Settings snapshot(float localGain,boolean headphones){return new Dsp.Settings(enabled,eq,gains,bass,compressor,ceiling,normalization?localGain:0,spatial==2||spatial==1&&(profile==1||profile==0&&headphones));}
}
