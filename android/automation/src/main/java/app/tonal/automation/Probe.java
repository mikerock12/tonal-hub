package app.tonal.automation;
import android.app.Instrumentation;import android.os.Bundle;import android.graphics.Rect;import android.view.accessibility.AccessibilityNodeInfo;import org.json.*;
/** Test-only UIAutomation lives in a separate package, so inspecting UI never restarts a player. */
public final class Probe extends Instrumentation {
 Bundle args;public void onCreate(Bundle b){super.onCreate(b);args=b;start();}
 public void onStart(){Bundle out=new Bundle();try{AccessibilityNodeInfo root=null;for(int n=0;n<15&&root==null;n++){root=getUiAutomation().getRootInActiveWindow();if(root==null)Thread.sleep(100);}if(root==null)throw new IllegalStateException("No active UI");JSONArray rows=new JSONArray();collect(root,rows);out.putString("result",rows.toString());finish(0,out);}catch(Throwable e){out.putString("error",e.toString());finish(1,out);}}
 void collect(AccessibilityNodeInfo n,JSONArray rows)throws Exception{if(rows.length()>=1500)return;if(n.isVisibleToUser()){Rect r=new Rect();n.getBoundsInScreen(r);rows.put(new JSONObject().put("text",n.getText()==null?"":n.getText().toString()).put("content-desc",n.getContentDescription()==null?"":n.getContentDescription().toString()).put("resource-id",n.getViewIdResourceName()==null?"":n.getViewIdResourceName()).put("class",n.getClassName()==null?"":n.getClassName().toString()).put("bounds","["+r.left+","+r.top+"]["+r.right+","+r.bottom+"]").put("clickable",n.isClickable()));}for(int i=0;i<n.getChildCount();i++){AccessibilityNodeInfo child=n.getChild(i);if(child!=null){collect(child,rows);child.recycle();}}}
}
