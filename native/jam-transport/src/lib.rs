//! Native session HTTP transport only. No player, audio callback or device activation.
use librespot_core::{Session,error::ErrorKind};
use hyper::Method;
use serde_json::{Value,json};
use std::time::Duration;
use bytes::Bytes;
use http_body_util::BodyExt;
use hyper::Request;

fn atom(value: &str, min: usize, max: usize) -> bool {
    (min..=max).contains(&value.len()) && value.bytes().all(|c|c.is_ascii_alphanumeric()||c==b'-'||c==b'_')
}

/// Fixed endpoint allowlist. Callers cannot supply a host, arbitrary URL or headers.
pub fn endpoint(command: &Value) -> Option<(Method,String)> {
    let device=command.get("device").and_then(Value::as_str).unwrap_or("");
    match command.get("op")?.as_str()? {
        "current" => Some((Method::GET,"/social-connect/v2/sessions/current".into())),
        "start" if atom(device,20,128) => Some((Method::GET,format!("/social-connect/v2/sessions/current_or_new?activate=true&local_device_id={device}&type=REMOTE"))),
        "join" if atom(device,20,128) => {
            let token=command.get("token")?.as_str()?;
            atom(token,6,512).then(||(Method::POST,format!("/social-connect/v2/sessions/join/{token}?playback_control=listen_and_control&join_type=deeplinking&local_device_id={device}")))
        },
        "leave" => {let id=command.get("session")?.as_str()?;atom(id,6,128).then(||(Method::POST,format!("/social-connect/v3/sessions/{id}/leave")))},
        "end" if atom(device,20,128) => {
            let id=command.get("session")?.as_str()?;
            atom(id,6,128).then(||(Method::DELETE,format!("/social-connect/v3/sessions/{id}?local_device_id={device}")))
        },
        _ => None,
    }
}

/// Bounded, cancellable network work. Raw errors can contain private URLs: never return them.
pub async fn execute(session: &Session, command: &Value) -> Value {
    let mut result=execute_inner(session,command).await;
    if let Some(id)=command.get("request").and_then(Value::as_u64).filter(|id|*id<=i32::MAX as u64){result["request"]=json!(id);}
    result
}
async fn execute_inner(session: &Session, command: &Value) -> Value {
    let Some((method,path))=endpoint(command) else {return json!({"status":400,"error":"invalid_command"});};
    let response=tokio::time::timeout(Duration::from_secs(10),async {
        let base=session.spclient().base_url().await?;
        let first=send(session,&base,&method,&path).await?;
        if matches!(first.0,404|405|501|502|503)&&base!="https://guc-spclient.spotify.com" {
            let second=send(session,"https://guc-spclient.spotify.com",&method,&path).await?;
            if second.0!=404||first.0==404{return Ok(second);}
        }
        Ok::<_,librespot_core::Error>(first)
    }).await;
    match response {
        Err(_) => json!({"status":408,"error":"timeout"}),
        Ok(Err(e)) => {
            let code=match e.kind {ErrorKind::NotFound=>404,ErrorKind::PermissionDenied=>403,ErrorKind::Unauthenticated=>401,ErrorKind::ResourceExhausted=>429,_=>503};
            json!({"status":code,"error":"spotify_unavailable"})
        },
        Ok(Ok((code,_))) if !(200..300).contains(&code) => json!({"status":code,"error":"spotify_unavailable"}),
        Ok(Ok((_,bytes))) if bytes.is_empty() => json!({"status":204,"result":null}),
        Ok(Ok((code,bytes))) => match serde_json::from_slice::<Value>(&bytes) {
            Ok(value) => json!({"status":code,"result":value,"viewer":session.username()}),
            Err(_) => json!({"status":502,"error":"invalid_response"}),
        }
    }
}

// Allowlisted Spotify hosts only. Bound the stream before materializing JSON;
// no redirects or raw endpoint/error logging (invites are private).
async fn send(session:&Session,base:&str,method:&Method,path:&str)->Result<(u16,Vec<u8>),librespot_core::Error>{
    let token=session.login5().auth_token().await?;
    let mut request=Request::builder().method(method.clone()).uri(format!("{base}{path}"))
        .header("authorization",format!("{} {}",token.token_type,token.access_token))
        .header("accept","application/json").header("app-platform","Android").header("content-length","0");
    if let Ok(client)=session.spclient().client_token().await {request=request.header("client-token",client);}
    let response=session.http_client().request_fut(request.body(Bytes::new())?)?.await?;
    let code=response.status().as_u16();if !(200..300).contains(&code){return Ok((code,Vec::new()));}
    if response.headers().get("content-length").and_then(|v|v.to_str().ok()).and_then(|s|s.parse::<u64>().ok()).is_some_and(|n|n>512*1024){return Ok((413,Vec::new()));}
    let mut body=response.into_body();let mut bytes=Vec::new();
    while let Some(frame)=body.frame().await{let frame=frame?;if let Some(data)=frame.data_ref(){if bytes.len()+data.len()>512*1024{return Ok((413,Vec::new()));}bytes.extend_from_slice(data);}}
    Ok((code,bytes))
}

#[cfg(test)] mod tests {
    use super::*;
    #[test] fn restrict_endpoints(){
        assert!(endpoint(&json!({"op":"current"})).is_some());
        assert!(endpoint(&json!({"op":"https://example.org"})).is_none());
        assert!(endpoint(&json!({"op":"join","device":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","token":"abc?bad"})).is_none());
        assert!(endpoint(&json!({"op":"join","device":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","token":"Abc_123-"})).is_some());
        assert!(endpoint(&json!({"op":"start","device":"../escape"})).is_none());
        assert!(endpoint(&json!({"op":"kick"})).is_none());
        assert!(endpoint(&json!({"op":"end","device":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","session":"../../bad"})).is_none());
    }
}
