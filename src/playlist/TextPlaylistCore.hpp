#pragma once
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Portable business logic: no Qt, JVM, React, network, tokens or audio state.
// Adapted from Maicon Nunes' PlaylistSpotify parser/search rules.
// 0.18.2: typed decisions (artist identity gate, version policy, availability),
// progressive artist-centred lookup and auditable reasons. See docs/TEXT_MATCHING_LYRIC_STYLE_0182.md.
// 0.18.3: complete paginated discography (albums, singles, compilations, appears_on), Part/Pt/roman
// numerals, duplicate artist profiles compared by discography, pinned artist IDs, honest SEARCH_INCOMPLETE.
// See docs/TEXT_DRAFTS_DISCOGRAPHY_0183.md.
namespace tonal::playlist {
struct Song { std::string artist, title, original; };
struct Parsed { std::string detectedArtist; std::vector<Song> songs; };

// Version found in parentheses/brackets or after " - ". Studio = no version qualifier.
enum class Version { Studio, Live, Acoustic, Demo, Remix, Instrumental, Karaoke, Cover, ReRecording, Edit, Alternate };
struct Title {
    std::string original, normalized, core;   // core: normalized title without non-essential qualifiers; Pt./roman numerals canonical
    std::vector<std::string> qualifiers;      // removed groups, original text
    Version version = Version::Studio;
    bool annotated = false;                   // equivalent-only qualifiers (remaster, feat., edition, credit)
    bool coverMarker = false;                 // karaoke / tribute / "in the style of" / cover
};

// Decision classes (TONALIFY spec section 5 / V3 section 5). Pending = lookup not finished yet.
enum class Match { Pending, Exact, Equivalent, Alternate, Cover, WrongArtist, WrongSong, Ambiguous, NotFound, Unavailable, LookupError, Incomplete, ArtistAmbiguous };

struct Candidate {
    std::string id, uri, title, album, albumType, releaseDate, restriction, linkedFrom, source;
    std::vector<std::string> artists, artistIds;   // performers in Spotify order (artists[0] = primary)
    int popularity = -1, durationMs = 0, playable = -1;   // playable: -1 unknown, 0 no, 1 yes
    std::string artist() const;                    // ", "-joined names for display
};
struct ArtistHit { std::string id, name; };
struct AlbumHit { std::string id, name, type, releaseDate, image, artistId; bool appearsOn = false; };

// Identity evidence from outside one search result.
struct Context {
    std::set<std::string> sameArtistIds;   // profiles with the requested name proven to be the same act (shared discography)
    std::string preferredId, preferredReason;   // pinned by the user (or by the same act's main profile)
    std::string sameArtistNote;                 // why those profiles count as one act (shown in the reason)
};

struct Evaluation { int index = -1; Match match = Match::NotFound; bool safe = false; double score = 0; std::string reason; };
struct Decision {
    Match match = Match::Pending;
    int choice = -1;            // index into ordered (not into the pool); -1 = nothing legitimate to preselect
    bool autoSelect = false;    // safeForAutoSelect of the chosen candidate
    std::string reason;
    std::vector<Evaluation> ordered;   // requested-artist results first; other artists last
};

Parsed parse(const std::string &text, int format = 0, const std::string &defaultArtist = {});
std::string normalize(const std::string &text);
Title title(const std::string &text);
bool sameArtist(const std::string &requested, const std::string &credited);
const char *code(Match);
Match matchFromCode(const std::string &);
std::string label(Match);
bool requestedArtistResult(Match);   // ordering group: true for results attributed to the requested artist name
// "requested", "versions" (other versions/editions of the same artist), "homonyms" (profiles to review), "other".
const char *group(Match);
// artists: exact-name artist hits from the artist search; resolved = that search already ran.
Decision decide(const Song &, const std::vector<Candidate> &, const std::vector<ArtistHit> &artists = {}, bool resolved = false, const Context &context = {});
std::vector<std::string> queries(const Song &);
// Duplicate key of a reference before any lookup: artist without article + canonical title core.
std::string referenceKey(const Song &);
// Stable fingerprint of a pasted batch (order-sensitive), to detect the same list added twice.
std::string fingerprint(const std::vector<Song> &);

struct Request {
    enum Kind { None, Tracks, Artists, Albums, AlbumTracks } kind = None;
    std::string path;                                          // relative to /v1, e.g. "/search"
    std::vector<std::pair<std::string, std::string>> query;   // unencoded
    std::string label;                                         // provenance shown in audit
    AlbumHit album;                                            // AlbumTracks: album that owns the tracks
};

// Progressive, bounded lookup for one requested track. Pure state machine: the platform executes
// each Request and feeds parsed results back. No network, clocks or threads here. Platforms cache
// discography responses per import, so the same artist is scanned once per list.
class Lookup {
public:
    explicit Lookup(Song song, std::string market = "from_token", int budget = 18, int discographyBudget = 240);
    Request next();
    // total: the response's "total" (album tracks paging); -1 when unknown.
    std::vector<int> acceptTracks(const std::vector<Candidate> &, int total = -1);   // pool index for each input (dedup by uri)
    void acceptArtists(const std::vector<ArtistHit> &);
    void acceptAlbums(const std::vector<AlbumHit> &, int total = -1);
    void fail(int status, const std::string &message);
    void seedArtists(const std::vector<ArtistHit> &);   // per-import cache: same artist on many lines
    void setContext(const Context &c) { context_ = c; safeKey_ = -1; }
    bool artistsResolved() const { return resolved_; }
    const std::vector<ArtistHit> &artists() const { return artists_; }
    const std::vector<Candidate> &pool() const { return pool_; }
    const Song &song() const { return song_; }
    Decision decision() const;
    Context effectiveContext() const;   // context + profiles proven equal by shared discography titles
    bool finished() const { return finished_; }
    bool discographyComplete() const { return exhausted_; }
    int releasesChecked() const { return releasesChecked_; }
    int backoffMs() const;   // > 0 when the last request failed transiently and will be repeated: wait this long (+ jitter)
    int requests() const { return used_; }
private:
    Request make(Request::Kind, std::string path, std::vector<std::pair<std::string, std::string>>, std::string label);
    bool hasRequestedArtistTrack() const;
    bool safeNow();
    Song song_; std::string market_; int budget_, discoBudget_, used_ = 0, searchUsed_ = 0, discoUsed_ = 0, stage_ = 0, error_ = 0;
    std::vector<Candidate> pool_; std::vector<ArtistHit> artists_; std::vector<AlbumHit> albums_;
    std::vector<std::string> text_, discoIds_; size_t textIndex_ = 1, albumIndex_ = 0, discoArtist_ = 0;
    int listOffset_ = 0, listTotal_ = -1, trackOffset_ = 0, releasesChecked_ = 0;
    bool listAppears_ = false, resolved_ = false, finished_ = false, incomplete_ = false, albumsOrdered_ = false, exhausted_ = false, discoStarted_ = false, discoCut_ = false;
    std::string errorMessage_; Request current_; long safeKey_ = -1; bool safe_ = false, retry_ = false; int attempts_ = 0;
    std::map<std::string, std::set<std::string>> titlesById_;   // every title core seen per artist profile
    std::map<std::string, int> releasesById_;
    Context context_;
};
}
