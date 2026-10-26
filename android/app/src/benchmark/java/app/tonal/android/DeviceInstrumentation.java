package app.tonal.android;
import android.app.Instrumentation;
import android.os.Bundle;
import android.view.accessibility.AccessibilityNodeInfo;
import java.util.*;

/** Device automation exists only in the separate benchmark APK. */
public final class DeviceInstrumentation extends Instrumentation {
    private Bundle args;
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);args=arguments;start();}
    @Override public void onStart(){
        Bundle result=new Bundle();
        try{
            if(args.getString("action","").equals("readerResumeTests")){result.putString("result",ReaderResumeTests.run(this));finish(0,result);return;}
            if(args.getString("action","").equals("historyStoryTests")){result.putString("result",HistoryStoryTests.run(this,args.getString("nativeModel","").equals("true")));finish(0,result);return;}
            if(args.getString("action","").equals("companionImport")){final String[] text={""};runOnMainSync(()->{try{MusicStore store=new MusicStore(getTargetContext(),"interop-library.json");store.restoreBackup(new String(java.nio.file.Files.readAllBytes(new java.io.File(getTargetContext().getFilesDir(),"companion-cpp-backup.json").toPath()),java.nio.charset.StandardCharsets.UTF_8));if(store.favorites().length()==0)throw new AssertionError("Missing imported favorite");text[0]="CPP_BACKUP_IMPORT_PASS";}catch(Exception e){throw new RuntimeException(e);}});result.putString("result",text[0]);finish(0,result);return;}
            if(args.getString("action","").equals("lyricPipelineTests")){result.putString("result",LyricPipelineTests.run(this));finish(0,result);return;}
            if(args.getString("action","").equals("songFeatureTests")){result.putString("result",SongFeatureTests.run(this));finish(0,result);return;}
            if(args.getString("action","").equals("coreTests")){result.putString("result",new CoreTests().run(this).toString());finish(0,result);return;}
            if(args.getString("action","").equals("playbackSafetyTests")){result.putString("result",new CoreTests().run(this,true).toString());finish(0,result);return;}
            android.app.UiAutomation automation=getUiAutomation();
            AccessibilityNodeInfo root=null;
            for(int attempt=0;attempt<20&&root==null;attempt++){
                Thread.sleep(250);
                root=automation.getRootInActiveWindow();
            }
            if(root==null)throw new IllegalStateException("No active window");
            String action=args.getString("action","dump");
            if(action.equals("click")){
                List<AccessibilityNodeInfo> matches=root.findAccessibilityNodeInfosByText(args.getString("text"));boolean clicked=false;
                for(AccessibilityNodeInfo node:matches){if(!node.isVisibleToUser())continue;AccessibilityNodeInfo parent=node;
                    for(int depth=0;parent!=null&&depth<6;depth++,parent=parent.getParent())if(parent.isClickable()&&parent.isEnabled()){
                        clicked=parent.performAction(AccessibilityNodeInfo.ACTION_CLICK);break;
                    }
                    if(clicked)break;
                }
                if(!clicked)throw new IllegalStateException("Visible enabled button not found");
                result.putString("result","CLICK_OK");
            }else if(action.equals("setText")){
                AccessibilityNodeInfo input=findInput(root);
                if(input==null)throw new IllegalStateException("Input not found");
                Bundle value=new Bundle();value.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE,args.getString("value"));
                if(!input.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT,value))throw new IllegalStateException("Text action rejected");
                result.putString("result","SET_TEXT_OK");
            }else{ArrayList<String> labels=new ArrayList<>();collect(root,labels);result.putString("result",String.join(" | ",labels));}
            finish(0,result);
        }catch(Throwable error){result.putString("error",error.getMessage());finish(1,result);}
    }
    private AccessibilityNodeInfo findInput(AccessibilityNodeInfo node){
        if(node.isEditable()&&!node.isPassword()&&(args.getString("field","").isEmpty()?node.isVisibleToUser():args.getString("field").contentEquals(node.getContentDescription())))return node;
        for(int i=0;i<node.getChildCount();i++){AccessibilityNodeInfo child=node.getChild(i);if(child!=null){AccessibilityNodeInfo found=findInput(child);if(found!=null)return found;}}return null;
    }
    private void collect(AccessibilityNodeInfo node,List<String> labels){
        if(node.isVisibleToUser()&&!node.isPassword()&&!node.isEditable()&&node.getText()!=null)labels.add(node.getText().toString());
        for(int i=0;i<node.getChildCount();i++){AccessibilityNodeInfo child=node.getChild(i);if(child!=null)collect(child,labels);}
    }
}
