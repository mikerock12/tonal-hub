#pragma once
#include <string>
#include <vector>

// Portable business logic: no Qt, JVM, React, network, tokens or audio state.
// Adapted from Maicon Nunes' PlaylistSpotify parser/search rules.
namespace tonal::playlist {
struct Song { std::string artist, title, original; };
struct Parsed { std::string detectedArtist; std::vector<Song> songs; };
struct Candidate { std::string title, artist; int popularity = -1; };
struct Ranked { int index; double score, artistSimilarity, titleSimilarity; bool highConfidence; };
Parsed parse(const std::string &text, int format = 0, const std::string &defaultArtist = {});
std::string normalize(const std::string &text);
std::vector<Ranked> rank(const Song &, const std::vector<Candidate> &);
std::vector<std::string> queries(const Song &);
}
