use super::{ConnectState, Context, ContextType, Error, ResetContext, Spirc, SpircCommand, SpircTask};
use tokio::sync::oneshot;

fn track_uri(uri: &str) -> bool {
    uri.strip_prefix("spotify:track:").is_some_and(|id| {
        id.len() == 22 && id.bytes().all(|byte| byte.is_ascii_alphanumeric())
    })
}

impl Spirc {
    /// The first URI is the expected current track. It remains at index zero
    /// of the edited context, including when the upcoming list is cleared.
    pub fn edit_queue(&self, tracks: Vec<String>) -> Result<oneshot::Receiver<bool>, Error> {
        if tracks.is_empty() || tracks.len() > 500 || !tracks.iter().all(|uri| track_uri(uri)) {
            return Err(Error::invalid_argument("invalid queue edit"));
        }
        let (ack, result) = oneshot::channel();
        self.commands.send(SpircCommand::EditQueue(tracks, ack))?;
        Ok(result)
    }
}

impl SpircTask {
    pub(super) fn edit_queue(&mut self, tracks: Vec<String>) -> Result<bool, Error> {
        let changed = replace_upcoming(&mut self.connect_state, tracks)?;
        if changed {
            self.context_resolver.clear();
            self.update_state = true;
        }
        Ok(changed)
    }
}

// Operates only on the Connect state. The playback clock, buffering recovery
// and preloader are owned by the player and are left alone during an edit.
fn replace_upcoming(state: &mut ConnectState, tracks: Vec<String>) -> Result<bool, Error> {
    if !state.is_active() || !state.current_track(|track| {
        track.as_ref().zip(tracks.first()).is_some_and(|(track, expected)| &track.uri == expected)
    }) {
        return Ok(false);
    }

    state.reset_context(ResetContext::Completely);
    state.update_context(Context {
        uri: Some("spotify:web-api".into()),
        url: Some("context://spotify:web-api".into()),
        pages: vec![tracks.into()],
        ..Default::default()
    }, ContextType::Default)?;
    state.set_active_context(ContextType::Default);
    state.set_shuffle(false);
    state.set_next_tracks(Vec::new());
    state.set_current_track(0)?;
    state.reset_playback_to_position(Some(0))?;
    Ok(true)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::{Session, config::SessionConfig};
    use crate::model::SpircPlayStatus;
    use crate::state::ConnectConfig;

    #[tokio::test]
    async fn replace_queue_during_buffering_preserves_clock_and_repeat() {
        let session = Session::new(SessionConfig::default(), None);
        let mut state = ConnectState::new(ConnectConfig::default(), &session);
        state.set_active(true);
        let a = "spotify:track:1AAAAAAAAAAAAAAAAAAAAA";
        let b = "spotify:track:1BBBBBBBBBBBBBBBBBBBBB";
        let c = "spotify:track:1CCCCCCCCCCCCCCCCCCCCC";
        state.update_context(Context {
            uri: Some("spotify:web-api".into()),
            pages: vec![vec![a.to_owned(), b.to_owned()].into()],
            ..Default::default()
        }, ContextType::Default).unwrap();
        state.set_current_track(0).unwrap();
        state.update_position(41_000, 123_456);
        state.set_repeat_track(true);
        state.set_status(&SpircPlayStatus::LoadingPlay { position_ms: 41_000 });
        let before = state.player().clone();

        assert!(!replace_upcoming(&mut state, vec![b.to_owned()]).unwrap());
        assert!(replace_upcoming(&mut state, [a, c, b, c].map(str::to_owned).to_vec()).unwrap());
        let after = state.player();
        assert_eq!(after.track.uri, a);
        assert_eq!(after.next_tracks.iter().map(|t| t.uri.as_str()).collect::<Vec<_>>(), [c, b, c]);
        assert_eq!(after.position_as_of_timestamp, before.position_as_of_timestamp);
        assert_eq!(after.timestamp, before.timestamp);
        assert_eq!(after.is_buffering, before.is_buffering);
        assert_eq!(after.is_paused, before.is_paused);
        assert!(state.repeat_track());
        assert!(replace_upcoming(&mut state, vec![a.to_owned()]).unwrap());
        assert!(state.player().next_tracks.is_empty());
    }
}
