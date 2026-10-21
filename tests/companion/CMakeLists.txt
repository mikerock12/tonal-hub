cmake_minimum_required(VERSION 3.21)
project(TonalPersonalChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Sql Qml)
qt_standard_project_setup(REQUIRES 6.8)
get_filename_component(ROOT "../.." ABSOLUTE)
qt_add_executable(TonalCompanionChecks CompanionTests.cpp
    "${ROOT}/src/companion/MusicCompanion.h" "${ROOT}/src/companion/MusicCompanion.cpp"
    "${ROOT}/src/companion/PersonalDatabase.h" "${ROOT}/src/companion/PersonalDatabase.cpp" "${ROOT}/src/companion/PersonalInsights.cpp" "${ROOT}/src/insights/TinyMusicModel.cpp")
target_include_directories(TonalCompanionChecks PRIVATE "${ROOT}/src")
target_link_libraries(TonalCompanionChecks PRIVATE Qt6::Core Qt6::Network Qt6::Sql)
# Compile transport reconciliation in CI without requiring the native audio SDK.
# Full linked PCM/process regressions run in the local top-level CTest build.
add_library(TonalPlayerCompileCheck OBJECT "${ROOT}/src/Player.cpp")
set_target_properties(TonalPlayerCompileCheck PROPERTIES AUTOMOC OFF)
target_include_directories(TonalPlayerCompileCheck PRIVATE "${ROOT}/src")
target_link_libraries(TonalPlayerCompileCheck PRIVATE Qt6::Core Qt6::Network Qt6::Qml)
if(MSVC)
    target_compile_options(TonalCompanionChecks PRIVATE /utf-8)
    target_compile_options(TonalPlayerCompileCheck PRIVATE /utf-8)
endif()
qt_add_resources(TonalCompanionChecks "rules" PREFIX "/companion" BASE "${ROOT}" FILES "${ROOT}/resources/music-rules.json")
enable_testing()
add_test(NAME personal_library COMMAND TonalCompanionChecks)
add_executable(TonalTextPlaylistChecks ../playlist/ImportChecks.cpp
    "${ROOT}/src/playlist/TextPlaylistCore.cpp" "${ROOT}/src/playlist/TextPlaylistImport.cpp"
    "${ROOT}/src/playlist/TextPlaylistImport.hpp"
    "${ROOT}/src/companion/MusicCompanion.cpp" "${ROOT}/src/companion/MusicCompanion.h"
    "${ROOT}/src/companion/PersonalDatabase.cpp" "${ROOT}/src/companion/PersonalInsights.cpp"
    "${ROOT}/src/insights/TinyMusicModel.cpp")
target_include_directories(TonalTextPlaylistChecks PRIVATE "${ROOT}/src")
target_link_libraries(TonalTextPlaylistChecks PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql)
if(MSVC)
    target_compile_options(TonalTextPlaylistChecks PRIVATE /utf-8)
endif()
qt_add_resources(TonalTextPlaylistChecks "text_rules" PREFIX "/companion" BASE "${ROOT}" FILES "${ROOT}/resources/music-rules.json")
add_test(NAME text_playlist_import COMMAND TonalTextPlaylistChecks)
if(WIN32)
    add_executable(TonalPcmPipeChecks ../audio/PcmPipeChecks.cpp
        "${ROOT}/src/audio/NativePcmPipe.cpp")
    target_include_directories(TonalPcmPipeChecks PRIVATE "${ROOT}/src")
    add_test(NAME pcm_pipe_alignment COMMAND TonalPcmPipeChecks)
endif()

add_executable(TonalMixCoreChecks ../mix/CoreChecks.cpp "${ROOT}/src/playback/ListeningMixCore.cpp" "${ROOT}/src/playlist/TextPlaylistCore.cpp")
target_include_directories(TonalMixCoreChecks PRIVATE "${ROOT}/src")
if(MSVC)
 target_compile_options(TonalMixCoreChecks PRIVATE /utf-8)
endif()
add_test(NAME listening_mix_core COMMAND TonalMixCoreChecks)

add_executable(TonalDiscoveryEnrichmentChecks ../mix/EnrichmentChecks.cpp
    "${ROOT}/src/playback/DiscoveryLyrics.cpp" "${ROOT}/src/insights/TinyMusicModel.cpp")
target_include_directories(TonalDiscoveryEnrichmentChecks PRIVATE "${ROOT}/src")
target_link_libraries(TonalDiscoveryEnrichmentChecks PRIVATE Qt6::Core Qt6::Network)
if(MSVC)
 target_compile_options(TonalDiscoveryEnrichmentChecks PRIVATE /utf-8)
endif()
add_test(NAME discovery_lyrics_enrichment COMMAND TonalDiscoveryEnrichmentChecks)

qt_add_executable(TonalThemeCatalogChecks ../themes/ThemeCatalogChecks.cpp
    "${ROOT}/src/ThemeCatalog.h" "${ROOT}/src/ThemeCatalog.cpp")
target_include_directories(TonalThemeCatalogChecks PRIVATE "${ROOT}/src")
target_link_libraries(TonalThemeCatalogChecks PRIVATE Qt6::Core Qt6::Qml)
if(MSVC)
 target_compile_options(TonalThemeCatalogChecks PRIVATE /utf-8)
endif()
qt_add_resources(TonalThemeCatalogChecks "themes" PREFIX "/themes" BASE "${ROOT}" FILES "${ROOT}/resources/themes.json")
add_test(NAME theme_catalog COMMAND TonalThemeCatalogChecks)
