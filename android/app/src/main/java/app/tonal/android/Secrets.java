package app.tonal.android;

import android.content.Context;
import android.security.keystore.*;
import android.util.Base64;
import org.json.JSONObject;
import javax.crypto.*;
import javax.crypto.spec.GCMParameterSpec;
import java.security.KeyStore;

/** Tokens are encrypted with a non-exportable key, never stored in ordinary preferences. */
final class Secrets {
    private final Context context;
    private final String keyAlias,preferenceName;
    Secrets(Context context){this(context,false);}
    Secrets(Context context,boolean engine){this.context=context.getApplicationContext();keyAlias=engine?"tonal.engine.oauth.v1":"tonal.oauth.v1";preferenceName=engine?"engine-secrets":"secrets";}
    Secrets(Context context,String namespace){this.context=context.getApplicationContext();keyAlias="tonal."+namespace+".v1";preferenceName=namespace+"-secrets";}
    private javax.crypto.SecretKey key() throws Exception {
        KeyStore store=KeyStore.getInstance("AndroidKeyStore");store.load(null);
        if(!store.containsAlias(keyAlias)){
            KeyGenerator generator=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(keyAlias,KeyProperties.PURPOSE_ENCRYPT|KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build());
            generator.generateKey();
        }
        return (javax.crypto.SecretKey)store.getKey(keyAlias,null);
    }
    synchronized JSONObject load(){
        try {
            String value=context.getSharedPreferences(preferenceName,0).getString("oauth",null);if(value==null)return null;
            byte[] stored=Base64.decode(value,Base64.NO_WRAP);int n=stored[0]&255;
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,key(),new GCMParameterSpec(128,stored,1,n));
            return new JSONObject(new String(cipher.doFinal(stored,1+n,stored.length-1-n),java.nio.charset.StandardCharsets.UTF_8));
        }catch(Exception error){return null;}
    }
    synchronized void save(JSONObject tokens) throws Exception {
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,key());
        byte[] iv=cipher.getIV(),encrypted=cipher.doFinal(tokens.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
        byte[] output=new byte[1+iv.length+encrypted.length];output[0]=(byte)iv.length;
        System.arraycopy(iv,0,output,1,iv.length);System.arraycopy(encrypted,0,output,1+iv.length,encrypted.length);
        if(!context.getSharedPreferences(preferenceName,0).edit().putString("oauth",Base64.encodeToString(output,Base64.NO_WRAP)).commit())throw new java.io.IOException("Não foi possível salvar o login");
    }
    synchronized void clear(){context.getSharedPreferences(preferenceName,0).edit().clear().apply();}
}
