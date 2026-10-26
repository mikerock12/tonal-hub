use librespot_core::{Session,SessionConfig,cache::Cache};
use serde_json::{Value,json};
use std::{io::{self,BufRead,Write},time::Duration};

fn reply(value: Value){let mut out=io::stdout().lock();let _=writeln!(out,"{value}");let _=out.flush();}

#[tokio::main(flavor="multi_thread",worker_threads=1)]
async fn main(){
    let Some(cache_path)=std::env::args_os().nth(1) else {reply(json!({"status":400,"error":"missing_cache"}));return;};
    let path=std::path::PathBuf::from(cache_path);
    // Read existing credentials; never write them or create a new login/device.
    if !path.join("credentials.json").is_file(){reply(json!({"status":401,"error":"engine_login_required"}));return;}
    let Ok(cache)=Cache::new(Some(path),None,None,None) else {reply(json!({"status":401,"error":"cache_unavailable"}));return;};
    let Some(credentials)=cache.credentials() else {reply(json!({"status":401,"error":"engine_login_required"}));return;};
    let mut config=SessionConfig::default();config.client_id="65b708073fc0480ea92a077233ca87bd".into();
    let session=Session::new(config,None);
    match tokio::time::timeout(Duration::from_secs(30),session.connect(credentials,false)).await {
        Ok(Ok(()))=>{},_=>{reply(json!({"status":401,"error":"session_unavailable"}));return;}
    }
    let input=io::stdin();let mut locked=input.lock();
    loop {
        // Read one capped JSON line; private join tokens never appear in stderr.
        let mut line=Vec::new();let mut oversized=false;
        loop {let Ok(buf)=locked.fill_buf() else {session.shutdown();return;};if buf.is_empty(){session.shutdown();return;}
            let n=buf.iter().position(|b|*b==b'\n').map(|n|n+1).unwrap_or(buf.len());
            if line.len()+n>8192{oversized=true;}if !oversized{line.extend_from_slice(&buf[..n]);}
            let done=buf[n-1]==b'\n';locked.consume(n);if done{break;}
        }
        if oversized{reply(json!({"status":413,"error":"command_too_large"}));continue;}
        let Ok(command)=serde_json::from_slice::<Value>(&line) else {reply(json!({"status":400,"error":"invalid_json"}));continue;};
        if command.get("op").and_then(Value::as_str)==Some("close"){session.shutdown();return;}
        reply(tonal_jam_transport::execute(&session,&command).await);
    }
}
