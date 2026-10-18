package app.tonal.android;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.util.Base64;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.concurrent.*;
import java.util.function.Consumer;
import java.util.function.BooleanSupplier;
import org.json.*;

final class SpotifyClient {
    static final String REDIRECT="http://127.0.0.1:8898/callback";
    static final String ENGINE_CLIENT_ID="65b708073fc0480ea92a077233ca87bd";
    static final String ENGINE_REDIRECT="http://127.0.0.1:5588/login";
    static final String ENGINE_SCOPES="streaming user-read-playback-state user-modify-playback-state user-read-currently-playing";
    static final String ENGINE_CACHE="librespot-oauth-v2";
    static final String SCOPES="streaming user-read-private user-read-playback-state user-modify-playback-state user-read-currently-playing user-library-read user-library-modify playlist-modify-private playlist-modify-public playlist-read-private playlist-read-collaborative user-read-recently-played user-top-read user-follow-read ugc-image-upload";
    final ExecutorService worker=Executors.newFixedThreadPool(2);
    private final Activity activity;
    private volatile boolean closed;
    private static final ScheduledExecutorService deadlines=Executors.newSingleThreadScheduledExecutor(r->{Thread t=new Thread(r,"spotify-deadlines");t.setDaemon(true);return t;});
    private final Secrets secrets;
    private final boolean engine;
    private volatile JSONObject tokens;
    private volatile ServerSocket callback;
    SpotifyClient(Activity activity){this(activity,false);}
    SpotifyClient(Activity activity,boolean engine){this.activity=activity;this.engine=engine;secrets=new Secrets(activity,engine);tokens=secrets.load();}
    boolean loggedIn(){return tokens!=null;}
    static String encode(String s){try{return java.net.URLEncoder.encode(s,"UTF-8");}catch(java.io.UnsupportedEncodingException impossible){throw new AssertionError(impossible);}}
    private static String random(){byte[] bytes=new byte[32];new SecureRandom().nextBytes(bytes);return Base64.encodeToString(bytes,Base64.URL_SAFE|Base64.NO_WRAP|Base64.NO_PADDING);}
    private String clientId(){return engine?ENGINE_CLIENT_ID:activity.getSharedPreferences("settings",0).getString("clientId","");}
    void logout(){tokens=null;secrets.clear();}
    private void deliver(Consumer<String> listener,String value){activity.runOnUiThread(()->{if(!closed)listener.accept(value);});}
    void login(String id,Consumer<String> result){
        if(!id.matches("[0-9a-fA-F]{32}")){deliver(result,"Client ID inválido (32 caracteres hexadecimais).");return;}
        if(engine&&!ENGINE_CLIENT_ID.equals(id)){deliver(result,"Identificação do motor inválida.");return;}
        if(!engine)activity.getSharedPreferences("settings",0).edit().putString("clientId",id).apply();
        final String redirect=engine?ENGINE_REDIRECT:REDIRECT;
        final String scopes=engine?ENGINE_SCOPES:SCOPES;
        worker.execute(()->{
            try {
                String verifier=random(),state=random();
                String challenge=Base64.encodeToString(MessageDigest.getInstance("SHA-256").digest(verifier.getBytes(StandardCharsets.US_ASCII)),Base64.URL_SAFE|Base64.NO_PADDING|Base64.NO_WRAP);
                if(callback!=null)callback.close();
                ServerSocket server=new ServerSocket();server.setReuseAddress(true);server.bind(new InetSocketAddress("127.0.0.1",engine?5588:8898));server.setSoTimeout(300000);callback=server;
                Uri url=Uri.parse("https://accounts.spotify.com/authorize").buildUpon().appendQueryParameter("client_id",id).appendQueryParameter("response_type","code")
                    .appendQueryParameter("redirect_uri",redirect).appendQueryParameter("scope",scopes).appendQueryParameter("state",state)
                    .appendQueryParameter("code_challenge_method","S256").appendQueryParameter("code_challenge",challenge).build();
                activity.runOnUiThread(()->activity.startActivity(new Intent(Intent.ACTION_VIEW,url)));
                String code=null;
                while(code==null){
                    try(Socket socket=server.accept()){
                        socket.setSoTimeout(5000);BufferedReader reader=new BufferedReader(new InputStreamReader(socket.getInputStream(),StandardCharsets.US_ASCII));
                        StringBuilder requestLine=new StringBuilder();int character;
                        while(requestLine.length()<8192&&(character=reader.read())!=-1&&character!='\n')requestLine.append((char)character);
                        String first=requestLine.toString().trim();if(first.isEmpty()||first.length()>=8192)continue;
                        String[] parts=first.split(" ");if(parts.length<2)continue;
                        Uri returned=Uri.parse("http://127.0.0.1:"+server.getLocalPort()+parts[1]);
                        boolean valid=Uri.parse(redirect).getPath().equals(returned.getPath())&&state.equals(returned.getQueryParameter("state"));
                        String message=valid?"Pode voltar ao Tonal.":"Resposta inválida; volte à autorização do Spotify.";
                        byte[] body=("<!doctype html><meta charset=utf-8><title>Tonal</title><h1>Tonal</h1><p>"+message+"</p>").getBytes(StandardCharsets.UTF_8);
                        OutputStream output=socket.getOutputStream();output.write(("HTTP/1.1 "+(valid?"200 OK":"400 Bad Request")+"\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "+body.length+"\r\nConnection: close\r\n\r\n").getBytes(StandardCharsets.US_ASCII));output.write(body);output.flush();
                        if(valid){if(returned.getQueryParameter("error")!=null)throw new IOException("Autorização cancelada.");code=returned.getQueryParameter("code");}
                    }
                }
                server.close();callback=null;
                JSONObject value=request("https://accounts.spotify.com/api/token","POST","grant_type=authorization_code&code="+encode(code)+"&redirect_uri="+encode(redirect)+"&client_id="+encode(id)+"&code_verifier="+encode(verifier),null);
                store(value,null);deliver(result,"Login concluído");
            }catch(Exception error){deliver(result,"Login: "+error.getMessage());}
            finally{ServerSocket server=callback;if(server!=null){try{server.close();}catch(IOException ignored){}callback=null;}}
        });
    }
    private synchronized void store(JSONObject value,String previousRefresh) throws Exception {
        if(!value.has("access_token"))throw new IOException("Token ausente");
        if(!value.has("refresh_token")&&previousRefresh!=null)value.put("refresh_token",previousRefresh);
        value.put("expires_at",System.currentTimeMillis()+value.optLong("expires_in",3600)*1000);
        secrets.save(value);tokens=value;
    }
    synchronized String accessToken() throws Exception {
        JSONObject current=tokens;if(current==null)throw new IOException("Faça login primeiro.");
        if(current.optLong("expires_at")<System.currentTimeMillis()+60000){
            String refresh=current.optString("refresh_token");
            JSONObject value=request("https://accounts.spotify.com/api/token","POST","grant_type=refresh_token&refresh_token="+encode(refresh)+"&client_id="+encode(clientId()),null);
            store(value,refresh);
        }
        return tokens.getString("access_token");
    }
    void get(String path,Consumer<JSONObject> result,Consumer<String> failure){
        worker.execute(()->{try{JSONObject json=request(apiUrl(path),"GET",null,accessToken());activity.runOnUiThread(()->{if(!closed)result.accept(json);});}catch(Exception error){deliver(failure,error.getMessage());}});
    }
    // Paging links come from Spotify. Never send a bearer token to another origin.
    static String apiUrl(String path) throws IOException {
        URI uri=URI.create(path.contains("://")?path:"https://api.spotify.com/v1/"+path);
        if(!"https".equals(uri.getScheme())||!"api.spotify.com".equals(uri.getHost())||uri.getPort()!=-1||uri.getUserInfo()!=null||!uri.getPath().startsWith("/v1/"))throw new IOException("Endereço de catálogo inválido");
        return uri.toString();
    }
    void mutate(String method,String path,JSONObject body,Consumer<JSONObject> result,Consumer<String> failure){
        worker.execute(()->{try{JSONObject json=request(apiUrl(path),method,body==null?null:body.toString(),accessToken(),true);activity.runOnUiThread(()->{if(!closed)result.accept(json);});}catch(Exception error){deliver(failure,error.getMessage());}});
    }
    static final class ApiFailure extends IOException {
        final int status,retryAfter;
        ApiFailure(int s,int retry,String message){super(message);status=s;retryAfter=retry;}
    }
    void importCall(String method,String path,String payload,String mime,Consumer<JSONObject> result,Consumer<String> failure,BooleanSupplier valid){importCall(method,path,payload,mime,result,failure,valid,0);}
    private void importCall(String method,String path,String payload,String mime,Consumer<JSONObject> result,Consumer<String> failure,BooleanSupplier valid,int attempt){
        if(closed||!valid.getAsBoolean())return;
        worker.execute(()->{if(!valid.getAsBoolean())return;try{
            JSONObject value=requestContent(apiUrl(path),method,payload,accessToken(),mime);
            activity.runOnUiThread(()->{if(!closed&&valid.getAsBoolean())result.accept(value);});
        }catch(Exception error){
            if(error instanceof ApiFailure){ApiFailure api=(ApiFailure)error;
                if(api.status==401&&attempt<1){synchronized(this){if(tokens!=null)MusicStore.put(tokens,"expires_at",0);}new android.os.Handler(android.os.Looper.getMainLooper()).post(()->importCall(method,path,payload,mime,result,failure,valid,attempt+1));return;}
                if(api.status==429&&attempt<3){new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(()->importCall(method,path,payload,mime,result,failure,valid,attempt+1),Math.max(1,Math.min(60,api.retryAfter))*1000L);return;}
            }
            activity.runOnUiThread(()->{if(!closed&&valid.getAsBoolean())failure.accept(error.getMessage());});
        }});
    }
    static JSONObject request(String url,String method,String form,String token) throws Exception {return request(url,method,form,token,false);}
    private static JSONObject request(String url,String method,String form,String token,boolean jsonBody) throws Exception {return requestContent(url,method,form,token,jsonBody?"application/json":"application/x-www-form-urlencoded");}
    private static JSONObject requestContent(String url,String method,String form,String token,String mime) throws Exception {
        HttpURLConnection connection=(HttpURLConnection)new URL(url).openConnection();
        ScheduledFuture<?> deadline=deadlines.schedule(connection::disconnect,url.contains("/api/token")?20:30,TimeUnit.SECONDS);
        try {
            connection.setConnectTimeout(15000);connection.setReadTimeout(15000);connection.setRequestMethod(method);connection.setInstanceFollowRedirects(false);
            if(token!=null)connection.setRequestProperty("Authorization","Bearer "+token);
            if(form!=null){connection.setDoOutput(true);connection.setRequestProperty("Content-Type",mime);try(OutputStream output=connection.getOutputStream()){output.write(form.getBytes(StandardCharsets.UTF_8));}}
            int code=connection.getResponseCode();InputStream input=code<400?connection.getInputStream():connection.getErrorStream();
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();if(input!=null){try(input){byte[] block=new byte[8192];int count;while((count=input.read(block))!=-1){if(bytes.size()+count>4*1024*1024)throw new IOException("Resposta muito grande");bytes.write(block,0,count);}}}
            String body=new String(bytes.toByteArray(),StandardCharsets.UTF_8);
            if(code>=400){String detail="";try{JSONObject error=new JSONObject(body);Object e=error.opt("error");detail=e instanceof JSONObject?((JSONObject)e).optString("message"):error.optString("error_description");}catch(JSONException ignored){}throw new ApiFailure(code,connection.getHeaderFieldInt("Retry-After",1),"Spotify HTTP "+code+(detail.isEmpty()?"":": "+detail));}
            if(body.trim().isEmpty())return new JSONObject();Object parsed=new JSONTokener(body).nextValue();return parsed instanceof JSONArray?new JSONObject().put("items",parsed):(JSONObject)parsed;
        }finally{deadline.cancel(false);connection.disconnect();}
    }
    void close(){closed=true;try{if(callback!=null)callback.close();}catch(IOException ignored){}worker.shutdownNow();}
}
