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

const ARTIST_OVERVIEW_HASH: &str = "4bc52527bb77a5f8bbb9afe491e9aa725698d29ab73bff58d49169ee29800167";

fn count(value: Option<&Value>) -> Option<u64> {
    match value? { Value::String(text) => text.parse().ok(), other => other.as_u64() }
}

fn public_artist_stats(body: &Value) -> Value {
    let artist=body.pointer("/data/artistUnion");
    let stats=artist.and_then(|value|value.get("stats"));
    let mut cities=Vec::new();
    if let Some(rows)=stats.and_then(|value|value.pointer("/topCities/items")).and_then(Value::as_array) {
        for row in rows.iter().take(5) {
            let city=row.get("city").and_then(Value::as_str).unwrap_or("");
            let country=row.get("country").and_then(Value::as_str).unwrap_or("");
            let listeners=count(row.get("numberOfListeners")).unwrap_or(0);
            if !city.is_empty()&&city.len()<=120&&country.len()==2&&listeners>0 {cities.push(json!({"city":city,"country":country,"listeners":listeners}));}
        }
    }
    let mut play_counts=serde_json::Map::new();let mut top_tracks=Vec::new();
    if let Some(rows)=artist.and_then(|value|value.pointer("/discography/topTracks/items")).and_then(Value::as_array) {
        for row in rows.iter().take(10) {
            let track=row.get("track").unwrap_or(row);
            let uri=track.get("uri").and_then(Value::as_str).unwrap_or("");
            if uri.len()!=36||!uri.starts_with("spotify:track:") {continue;}
            if let Some(plays)=count(track.get("playcount")) {play_counts.insert(uri.to_string(),json!(plays));top_tracks.push(json!(uri));}
        }
    }
    json!({
        "source":"Spotify • página pública do artista",
        "followers":count(stats.and_then(|value|value.get("followers"))),
        "monthlyListeners":count(stats.and_then(|value|value.get("monthlyListeners"))),
        "worldRank":count(stats.and_then(|value|value.get("worldRank"))),
        "cities":cities,"playCounts":play_counts,"topTracks":top_tracks,
    })
}

async fn artist_stats(session:&Session,uri:&str)->Value {
    let query=json!({"operationName":"queryArtistOverview","variables":{"uri":uri,"locale":"","includePrerelease":true},"extensions":{"persistedQuery":{"version":1,"sha256Hash":ARTIST_OVERVIEW_HASH}}});
    let response=tokio::time::timeout(Duration::from_secs(12),async {
        let login=session.login5().auth_token().await?;let client=session.spclient().client_token().await?;
        let bytes=serde_json::to_vec(&query).unwrap_or_default();
        let request=Request::builder().method(Method::POST).uri("https://api-partner.spotify.com/pathfinder/v1/query")
            .header("authorization",format!("{} {}",login.token_type,login.access_token)).header("client-token",client)
            .header("app-platform","Android").header("accept","application/json").header("content-type","application/json")
            .body(Bytes::from(bytes))?;
        let response=session.http_client().request_fut(request)?.await?;let code=response.status().as_u16();
        if !(200..300).contains(&code){return Ok::<_,librespot_core::Error>((code,Vec::new()));}
        let mut body=response.into_body();let mut bytes=Vec::new();
        while let Some(frame)=body.frame().await{let frame=frame?;if let Some(data)=frame.data_ref(){if bytes.len()+data.len()>1024*1024{return Ok((413,Vec::new()));}bytes.extend_from_slice(data);}}
        Ok((code,bytes))
    }).await;
    match response {
        Err(_)=>json!({"status":408,"error":"timeout"}),
        Ok(Err(_))=>json!({"status":503,"error":"spotify_unavailable"}),
        Ok(Ok((code,_))) if !(200..300).contains(&code)=>json!({"status":code,"error":"spotify_unavailable"}),
        Ok(Ok((code,bytes)))=>match serde_json::from_slice::<Value>(&bytes){Ok(body)=>json!({"status":code,"result":public_artist_stats(&body)}),Err(_)=>json!({"status":502,"error":"invalid_response"})},
    }
}

/// Bounded, cancellable network work. Raw errors can contain private URLs: never return them.
pub async fn execute(session: &Session, command: &Value) -> Value {
    let mut result=execute_inner(session,command).await;
    if let Some(id)=command.get("request").and_then(Value::as_u64).filter(|id|*id<=i32::MAX as u64){result["request"]=json!(id);}
    result
}
async fn execute_inner(session: &Session, command: &Value) -> Value {
    if command.get("op").and_then(Value::as_str)==Some("artistStats") {
        let uri=command.get("uri").and_then(Value::as_str).unwrap_or("");
        let valid=uri.len()==37&&uri.starts_with("spotify:artist:")&&uri[15..].bytes().all(|c|c.is_ascii_alphanumeric());
        return if valid {artist_stats(session,uri).await} else {json!({"status":400,"error":"invalid_artist"})};
    }
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
    #[test] fn artist_statistics_are_public_and_bounded(){
        let body=json!({"data":{"artistUnion":{
            "stats":{"followers":363397,"monthlyListeners":467589,"worldRank":123,"topCities":{"items":[{"city":"Melbourne","country":"AU","numberOfListeners":9470}]}},
            "discography":{"topTracks":{"items":[{"track":{"uri":"spotify:track:abcdefghijklmnopqrstuv","playcount":"2889055"}}]}}
        }}});
        let stats=public_artist_stats(&body);
        assert_eq!(stats["monthlyListeners"],467589);assert_eq!(stats["cities"][0]["listeners"],9470);assert_eq!(stats["playCounts"]["spotify:track:abcdefghijklmnopqrstuv"],2889055);
    }
}
