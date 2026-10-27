package app.tonal.android;

import android.app.*;
import android.content.*;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Session controls and in-app notices. No overlay permission, credentials or persistent guest history. */
final class JamClient implements AutoCloseable {
    private final MainActivity host;private final SpotifyClient api;private final Ui ui;
    private final Handler main=new Handler(Looper.getMainLooper());private final Consumer<String> result=this::result;
    private JSONObject session=new JSONObject();private String status="Jam experimental • amigos participam pelo Spotify",op="",queueAnchor="";
    private ArrayList<String> queue=new ArrayList<>();private boolean busy,closed,baseline,queueBusy;private int requestId,pending;
    private long quietUntil,cooldown;private Runnable redraw=()->{};private Dialog dialog;private PopupWindow popup;private final ArrayDeque<String> notices=new ArrayDeque<>();
    JamClient(MainActivity host,SpotifyClient api){this.host=host;this.api=api;ui=new Ui(host);NativeBridge.jamListener=result;}
    boolean active(){return session.optBoolean("active");}
    boolean capacityExceeded;
    private void reconcile(JSONObject data,String anchor){
        if(SystemClock.elapsedRealtime()<quietUntil||!host.isCurrentTrack(anchor))return;
        ArrayList<JSONObject> observed=new ArrayList<>();JSONObject current=data.optJSONObject("currently_playing");if(current==null)return;observed.add(current);
        for(Object v:MusicStore.values(MusicStore.array(data,"queue"))){if(observed.size()>=101)break;if(v instanceof JSONObject&&!((JSONObject)v).optString("uri").isEmpty())observed.add((JSONObject)v);}
        ArrayList<JSONObject> known=new ArrayList<>(host.orderedQueue);String[] old=new String[known.size()],now=new String[observed.size()];if(old.length>500){capacityExceeded=true;return;}
        for(int i=0;i<old.length;i++)old[i]=known.get(i).optString("uri");for(int i=0;i<now.length;i++)now[i]=observed.get(i).optString("uri");
        int[] alignment=ListeningMixNative.alignQueue(old,now);if(alignment==null||alignment.length!=now.length+1)return;
        for(int i=0;i<now.length;i++)if(alignment[i]>=0){JSONObject source=known.get(alignment[i]),target=observed.get(i);for(Iterator<String> keys=source.keys();keys.hasNext();){String key=keys.next();if(key.startsWith("_mix"))MusicStore.put(target,key,source.opt(key));}}
        int suffix=alignment[now.length];if(suffix<0||suffix>known.size())return;for(int index=suffix;index<known.size();index++){boolean used=false;for(int n=0;n<now.length;n++)if(alignment[n]==index){used=true;break;}if(!used)observed.add(known.get(index));}capacityExceeded=observed.size()>500;if(capacityExceeded)return;
        boolean same=known.size()==observed.size();if(same)for(int i=0;i<known.size();i++)if(!known.get(i).optString("uri").equals(observed.get(i).optString("uri"))){same=false;break;}
        if(!same){host.cancelMix();host.orderedQueue.clear();host.orderedQueue.addAll(observed);host.orderedCursor=0;}
    }
    void quiet(){baseline=false;quietUntil=SystemClock.elapsedRealtime()+3500;}
    // Notices that arrive while the app is in the background wait (at most eight)
    // and appear when it returns; the in-app popup never draws over other apps.
    void foreground(boolean value){main.removeCallbacks(poll);if(value&&active())main.post(poll);if(value){check();if(popup==null)nextNotice();}else{if(popup!=null)popup.dismiss();popup=null;}}
    private long lastCheck;
    /** Finds an active Jam without opening the dialog (engine ready, app in front), at most every two minutes. */
    void check(){if(closed||busy||active()||!host.isForeground()||!NativeBridge.ready)return;long now=SystemClock.elapsedRealtime();if(lastCheck!=0&&now-lastCheck<120000)return;lastCheck=now;request("current",null);}
    /** Share sheet chosen by the user; nothing is sent automatically. */
    private void shareInvite(){String link=session.optString("link");if(link.isEmpty()||closed)return;Intent send=new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT,"Entre no meu Jam no Spotify: "+link);try{host.startActivity(Intent.createChooser(send,"Convidar para o Jam"));}catch(ActivityNotFoundException e){host.notice("Convite copiado");}}
    void show(){
        LinearLayout content=ui.column();ui.pad(content,18,12);content.addView(ui.text("Iniciar Jam cria a sessão; seus amigos entram pelo convite (link). Ao iniciar, o Tonalify abre o compartilhamento para você escolher para quem enviar. A entrada é informada, sem aprovação prévia. Protocolo experimental.",14,Ui.MUTED));
        TextView state=ui.text(status,15,Ui.TEXT);content.addView(state);LinearLayout controls=ui.row();
        TextView start=ui.action("Iniciar Jam",()->request("start",null));ui.weight(controls,start);
        TextView refresh=ui.action("Atualizar",()->request("current",null));ui.weight(controls,refresh);content.addView(controls);
        TextView share=ui.action("Compartilhar convite",this::shareInvite);content.addView(share);
        TextView copy=ui.action("Copiar convite",()->{String link=session.optString("link");if(!link.isEmpty()){host.getSystemService(android.content.ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("Convite Jam",link));host.notice("Convite copiado");}});content.addView(copy);
        TextView leave=ui.action("Sair / encerrar Jam",()->request(session.optBoolean("host")?"end":"leave",null));content.addView(leave);
        LinearLayout members=ui.column();content.addView(members);
        EditText invite=new EditText(host);invite.setTextColor(Ui.TEXT);invite.setHintTextColor(Ui.MUTED);invite.setHint("Cole o convite do Jam");invite.setSingleLine(true);invite.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(600)});content.addView(invite);
        TextView join=ui.action("Entrar pelo convite",()->{
            String token=invite.getText().toString().trim();Uri uri=Uri.parse(token);
            if("https".equals(uri.getScheme())&&"open.spotify.com".equals(uri.getHost())&&uri.getPath()!=null&&uri.getPath().startsWith("/socialsession/"))token=uri.getLastPathSegment();
            else if(token.startsWith("spotify:socialsession:"))token=token.substring(22);
            if(token==null||!token.matches("[A-Za-z0-9_-]{6,512}")){host.notice("Use um convite open.spotify.com/socialsession ou o código do Jam.");return;}request("join",token);
        });content.addView(join);content.addView(ui.text("O Spotify pode exigir Premium. Avisos mostram a autoria apenas quando identificada pelo serviço. Nenhum convite é enviado automaticamente.",12,Ui.MUTED));
        ScrollView scroll=new ScrollView(host);scroll.addView(content);dialog=new AlertDialog.Builder(host).setTitle("Jam com amigos").setView(scroll).setPositiveButton("Fechar",null).create();
        redraw=()->{state.setText(status);start.setEnabled(!busy&&!active());refresh.setEnabled(!busy);join.setEnabled(!busy);leave.setVisibility(active()?View.VISIBLE:View.GONE);leave.setEnabled(!busy);copy.setVisibility(session.optString("link").isEmpty()?View.GONE:View.VISIBLE);share.setVisibility(copy.getVisibility());members.removeAllViews();for(Object v:MusicStore.values(MusicStore.array(session,"members")))members.addView(ui.text(((JSONObject)v).optString("name"),16,Ui.TEXT));};
        dialog.setOnDismissListener(d->{redraw=()->{};dialog=null;});dialog.show();redraw.run();request("current",null);
    }
    private void request(String operation,String token){
        if(closed||busy)return;if(SystemClock.elapsedRealtime()<cooldown){status="Aguarde um pouco antes de consultar o Jam novamente.";redraw.run();return;}
        if(!NativeBridge.ready){status="Conecte o motor Spotify do Tonalify antes de usar o Jam.";redraw.run();return;}
        JSONObject command=new JSONObject();MusicStore.put(command,"op",operation);MusicStore.put(command,"request",++requestId);if(token!=null)MusicStore.put(command,"token",token);if(operation.equals("end")||operation.equals("leave"))MusicStore.put(command,"session",session.optString("id"));
        pending=requestId;op=operation;busy=true;status="Consultando Jam…";redraw.run();NativeBridge.jamRequest(command.toString());main.removeCallbacks(timeout);main.postDelayed(timeout,14000);
    }
    private final Runnable timeout=()->{if(!busy)return;busy=false;pending=-1;status="Tempo de consulta Jam esgotado. Tente atualizar.";redraw.run();};
    private final Runnable poll=new Runnable(){public void run(){if(closed||!host.isForeground()||!active())return;if(!busy)request("current",null);main.postDelayed(this,5000);}};
    private void result(String json){
        if(closed)return;try{
            JSONObject response=new JSONObject(json);
            if("session_update".equals(response.optString("event"))){update(response.optJSONObject("update"));return;}
            if(!busy||response.optInt("request",-1)!=pending)return;busy=false;main.removeCallbacks(timeout);int code=response.optInt("status");
            if(code==404&&op.equals("current")){session=new JSONObject();baseline=false;status="Nenhum Jam ativo nesta conta.";main.removeCallbacks(poll);redraw.run();return;}
            if(code<200||code>=300){cooldown=SystemClock.elapsedRealtime()+(code==429?60000:code==503?15000:5000);status="Jam indisponível nesta sessão (HTTP "+code+").";main.removeCallbacks(poll);redraw.run();return;}
            if(op.equals("end")||op.equals("leave")){session=new JSONObject();baseline=false;status="Você saiu do Jam.";main.removeCallbacks(poll);redraw.run();return;}
            JSONObject next=parse(response.optJSONObject("result"),response.optString("viewer"));
            if(next==null){if(!op.equals("current")){request("current",null);return;}status="Formato de sessão Jam não reconhecido.";redraw.run();return;}
            if(session.optString("id").equals(next.optString("id"))){Set<String> old=new HashSet<>();for(Object v:MusicStore.values(MusicStore.array(session,"members")))old.add(((JSONObject)v).optString("id"));for(Object v:MusicStore.values(MusicStore.array(next,"members"))){JSONObject m=(JSONObject)v;if(!old.remove(m.optString("id")))notice(m.optString("name")+" entrou no Jam");}for(Object v:MusicStore.values(MusicStore.array(session,"members"))){JSONObject m=(JSONObject)v;if(old.contains(m.optString("id")))notice(m.optString("name")+" saiu do Jam");}}
            else baseline=false;
            session=next;status="Jam ativo • alterações confirmadas aparecem no player";redraw.run();if(op.equals("start")){status="Jam iniciado • envie o convite para seus amigos entrarem";redraw.run();shareInvite();}main.removeCallbacks(poll);if(host.isForeground())main.postDelayed(poll,5000);pollQueue();
        }catch(Exception invalid){busy=false;main.removeCallbacks(poll);status="Resposta Jam inválida.";redraw.run();}
    }
    /** Membership pushed by the service: who joined or left, as the service names them. */
    private void update(JSONObject update){
        if(update==null||closed)return;String reason=update.optString("reason"),id=update.optString("session");
        if(!id.matches("[A-Za-z0-9_-]{6,128}"))return;
        JSONArray members=new JSONArray();for(Object v:MusicStore.values(MusicStore.array(update,"members"))){if(!(v instanceof JSONObject)||members.length()>=128)continue;JSONObject m=(JSONObject)v;try{members.put(new JSONObject().put("id",m.optString("id")).put("name",m.optString("name").isEmpty()?"Participante":m.optString("name")));}catch(JSONException ignored){}}
        boolean known=id.equals(session.optString("id"));
        for(Object v:MusicStore.values(MusicStore.array(update,"updated"))){if(!(v instanceof JSONObject))continue;String name=((JSONObject)v).optString("name");if(name.isEmpty())name="Um participante";
            if(reason.equals("USER_JOINED"))notice(name+" entrou no Jam");else if(reason.equals("USER_LEFT"))notice(name+" saiu do Jam");else if(reason.equals("USER_KICKED"))notice(name+" foi removido do Jam");}
        if(reason.equals("SESSION_DELETED")||reason.equals("YOU_LEFT")||reason.equals("YOU_WERE_KICKED")){if(known){session=new JSONObject();baseline=false;main.removeCallbacks(poll);}notice(reason.equals("SESSION_DELETED")?"O Jam foi encerrado":reason.equals("YOU_WERE_KICKED")?"Você foi removido do Jam":"Você saiu do Jam");status="Nenhum Jam ativo nesta conta.";redraw.run();return;}
        // Keep the polled member list in step so the next poll does not repeat these notices.
        try{if(known||session.length()==0){if(!known){session=new JSONObject().put("id",id).put("active",true).put("host",update.optBoolean("owner")).put("link","");}session.put("members",members);}}catch(JSONException ignored){}
        redraw.run();if(!busy&&(session.optString("link").isEmpty()||!known))request("current",null);else if(host.isForeground()){main.removeCallbacks(poll);main.postDelayed(poll,5000);}
    }
    private static String text(JSONObject value,String... fields){for(String field:fields){Object v=value.opt(field);if(v instanceof String)return (String)v;}return "";}
    static JSONObject parse(JSONObject value)throws JSONException{return parse(value,"");}
    static JSONObject parse(JSONObject value,String viewer)throws JSONException{
        if(value==null)return null;JSONObject s=value.optJSONObject("session");if(s==null)s=value;String id=text(s,"session_id","sessionId","id");if(!id.matches("[A-Za-z0-9_-]{6,128}"))return null;
        JSONArray rows=s.optJSONArray("session_members");if(rows==null)rows=s.optJSONArray("sessionMembers");if(rows==null)rows=s.optJSONArray("members");if(rows==null||rows.length()>128)return null;JSONArray members=new JSONArray();Set<String> ids=new HashSet<>();
        for(Object v:MusicStore.values(rows)){if(!(v instanceof JSONObject))return null;JSONObject m=(JSONObject)v;String key=text(m,"id","username","user_id"),name=text(m,"display_name","displayName","name");if(name.isEmpty())name=text(m,"username");if(name.isEmpty())name="Participante";if(key.isEmpty()||key.length()>160||name.length()>160||!ids.add(key))return null;members.put(new JSONObject().put("id",key).put("name",name));}
        String token=text(s,"join_session_token","joinSessionToken"),owner=text(s,"session_owner_id","sessionOwnerId","owner_id");boolean ownerFlag=s.has("is_session_owner")?Boolean.TRUE.equals(s.opt("is_session_owner")):s.has("isSessionOwner")?Boolean.TRUE.equals(s.opt("isSessionOwner")):!viewer.isEmpty()&&viewer.equals(owner);return new JSONObject().put("id",id).put("active",true).put("host",ownerFlag).put("members",members).put("link",token.matches("[A-Za-z0-9_-]{6,512}")?"https://open.spotify.com/socialsession/"+token:"");
    }
    private void pollQueue(){
        if(queueBusy||!active()||!host.isForeground())return;queueBusy=true;String identity=session.optString("id");
        api.get("me/player/queue",data->{queueBusy=false;if(closed||!identity.equals(session.optString("id")))return;
            String current=MusicStore.object(data,"currently_playing").optString("uri");ArrayList<String> next=new ArrayList<>(),names=new ArrayList<>();for(Object v:MusicStore.values(MusicStore.array(data,"queue"))){if(next.size()>=100)break;JSONObject track=(JSONObject)v;if(!track.optString("uri").isEmpty()){next.add(track.optString("uri"));JSONArray who=track.optJSONArray("artists");String artist=who!=null&&who.length()>0?who.optJSONObject(0).optString("name"):"";names.add(artist.isEmpty()?track.optString("name"):track.optString("name")+" — "+artist);}}
            if(baseline&&current.equals(queueAnchor)&&SystemClock.elapsedRealtime()>=quietUntil&&!next.equals(queue)){ArrayList<String> old=new ArrayList<>(queue),added=new ArrayList<>();for(int i=0;i<next.size();i++){if(!old.remove(next.get(i))&&added.size()<3)added.add(names.get(i));}int removed=old.size();notice(!added.isEmpty()?"Adicionada à fila do Jam: "+String.join(" • ",added):removed>0?(removed==1?"Uma música saiu da fila do Jam":removed+" músicas saíram da fila do Jam"):"A ordem da fila do Jam mudou");}
            reconcile(data,current);queue=next;queueAnchor=current;baseline=true;
        },error->queueBusy=false);
    }
    private void notice(String text){if(closed)return;String bounded=text.substring(0,Math.min(600,text.length()));if(notices.contains(bounded))return;while(notices.size()>=8)notices.removeFirst();notices.addLast(bounded);if(popup==null&&host.isForeground())nextNotice();}
    private void nextNotice(){
        if(closed||!host.isForeground()||notices.isEmpty())return;
        TextView label=ui.text(notices.removeFirst(),14,Ui.TEXT);ui.pad(label,16,14);ui.rounded(label,Ui.TINT,14);int width=Math.min(ui.dp(360),host.getResources().getDisplayMetrics().widthPixels-ui.dp(32));
        PopupWindow view=new PopupWindow(label,width,-2,false);popup=view;view.setTouchable(false);view.setElevation(ui.dp(8));view.showAtLocation(host.getWindow().getDecorView(),Gravity.BOTTOM|Gravity.START,ui.dp(16),ui.dp(108));
        boolean animated=android.animation.ValueAnimator.areAnimatorsEnabled();label.setAlpha(animated?0:1);label.setTranslationY(animated?ui.dp(18):0);if(animated)label.animate().alpha(1).translationY(0).setDuration(220).start();
        main.postDelayed(()->{if(popup!=view)return;Runnable done=()->{view.dismiss();popup=null;nextNotice();};if(animated)label.animate().alpha(0).translationY(-ui.dp(8)).setDuration(220).withEndAction(done).start();else done.run();},3500);
    }
    public void close(){closed=true;main.removeCallbacksAndMessages(null);if(NativeBridge.jamListener==result)NativeBridge.jamListener=null;if(popup!=null)popup.dismiss();if(dialog!=null)dialog.dismiss();notices.clear();}
}
