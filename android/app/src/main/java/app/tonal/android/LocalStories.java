package app.tonal.android;

import android.content.Context;
import org.json.*;
import java.io.File;
import java.util.*;

/** Deterministic genre/mood summary. No model, network, inference or playback dependency. */
final class LocalStories {
    private final File modelDir,root;
    private Runnable observer;
    private int period=7;
    private String status="Resumo de gêneros e clima por regras locais, sem modelo.",summary="",sources="";
    private JSONArray items=new JSONArray();

    LocalStories(Context context,String module){
        modelDir=new File(context.getFilesDir(),"stories-model");root=new File(context.getCacheDir(),"stories-"+module);
        removeLegacyFiles();
    }
    private void removeLegacyFiles(){
        for(String name:new String[]{"Qwen3.5-2B-Q4_K_M.gguf","qwen2.5-0.5b-instruct-q4_k_m.gguf","verified.json","download.lock"})new File(modelDir,name).delete();
        File[] partials=modelDir.listFiles((dir,name)->name.startsWith("download-")&&name.endsWith(".part"));if(partials!=null)for(File file:partials)file.delete();
        for(String name:new String[]{"resume-v5.bin","resume-v5.bin.tmp"})new File(root,name).delete();
        File[] old=new File(root,"summaries").listFiles((dir,name)->name.endsWith(".json"));if(old!=null)for(File file:old)file.delete();
    }
    void setPlaying(boolean value){}
    void selectPeriod(int days){if(period!=days){period=days;summary=sources="";items=new JSONArray();notifyChanged();}}
    void resetSummary(){summary=sources="";items=new JSONArray();removeLegacyFiles();notifyChanged();}
    void observe(Runnable update){observer=update;}
    void detach(){observer=null;}
    private void notifyChanged(){if(observer!=null)observer.run();}
    JSONObject state(){JSONObject o=new JSONObject();MusicStore.put(o,"playbackPrioritized",false);MusicStore.put(o,"ready",true);MusicStore.put(o,"busy",false);MusicStore.put(o,"downloading",false);MusicStore.put(o,"status",status);MusicStore.put(o,"summary",summary);MusicStore.put(o,"sources",sources);MusicStore.put(o,"items",items);MusicStore.put(o,"partial",false);MusicStore.put(o,"progress",0);MusicStore.put(o,"legacyBytes",0);MusicStore.put(o,"modelBytes",0);MusicStore.put(o,"cacheBytes",0);MusicStore.put(o,"engine","Regras locais • sem modelo");return o;}
    void capture(JSONObject track,String lyrics){}
    void cancel(){status="Resumo local por regras; nenhuma inferência em andamento.";notifyChanged();}
    void download(){status="Nenhum modelo ou download é necessário.";notifyChanged();}
    void removeModel(){removeLegacyFiles();status="Os pesos antigos foram removidos. Este resumo não usa modelo.";notifyChanged();}

    void analyze(JSONArray tracks,int days){
        period=days;summary=sources="";items=new JSONArray();
        if(tracks.length()==0){status="Não há músicas ouvidas neste período.";notifyChanged();return;}
        LinkedHashMap<String,Integer> genres=new LinkedHashMap<>(),moods=new LinkedHashMap<>();
        String[] moodNames={"intenso","contemplativo","afetivo","festivo"};
        ArrayList<String> labels=new ArrayList<>();int counted=0;
        for(Object value:MusicStore.values(tracks)){
            if(!(value instanceof JSONObject)||counted>=8)continue;JSONObject track=(JSONObject)value;counted++;
            String genre=track.optString("genre").trim();if(genre.isEmpty())genre="Gênero não identificado";
            String mood="clima não identificado",bestMood="";double best=0;
            JSONObject tone=MusicStore.object(track,"tone");for(String name:moodNames){double score=tone.optDouble(name);if(score>best){best=score;bestMood=name;}}
            if(!bestMood.isEmpty())mood=bestMood;
            genres.put(genre,genres.getOrDefault(genre,0)+1);if(!bestMood.isEmpty())moods.put(bestMood,moods.getOrDefault(bestMood,0)+1);
            JSONObject item=new JSONObject();MusicStore.put(item,"title",track.optString("title"));MusicStore.put(item,"artist",track.optString("artist"));MusicStore.put(item,"accepted",true);MusicStore.put(item,"pending",false);MusicStore.put(item,"theme",genre+" • "+mood);MusicStore.put(item,"meaning","Contribui para o resumo com o gênero e a pista de clima já registrados nesta biblioteca.");items.put(item);
            labels.add(track.optString("title")+" — "+track.optString("artist"));
        }
        sources=String.join(" • ",labels);
        if(counted==0){status="Não há músicas ouvidas neste período.";notifyChanged();return;}
        String topGenres=top(genres,2),topMoods=top(moods,2);
        StringBuilder text=new StringBuilder("Neste período, os gêneros mais presentes são ");text.append(topGenres.isEmpty()?"ainda não identificados":topGenres).append(".");
        if(!topMoods.isEmpty())text.append(" As pistas de clima mais frequentes são ").append(topMoods).append(".");
        else text.append(" Ainda não há pistas de clima suficientes nestas faixas.");
        text.append(" Resumo por regras de ").append(counted).append(counted==1?" faixa, sem modelo de IA.":" faixas, sem modelo de IA.");
        summary=text.toString();status="Resumo local por regras • "+counted+(counted==1?" faixa":" faixas")+" • sem download ou inferência.";notifyChanged();
    }
    private static String top(LinkedHashMap<String,Integer> counts,int limit){ArrayList<Map.Entry<String,Integer>> rows=new ArrayList<>(counts.entrySet());rows.sort((a,b)->Integer.compare(b.getValue(),a.getValue()));ArrayList<String> names=new ArrayList<>();for(int i=0;i<Math.min(limit,rows.size());i++)names.add(rows.get(i).getKey());return String.join(" e ",names);}
}
