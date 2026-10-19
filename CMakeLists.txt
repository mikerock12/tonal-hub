cmake_minimum_required(VERSION 3.21)

project(Tonal VERSION 0.12.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.8 LTS ou mais novo (usamos Shape.CurveRenderer e loadFromModule)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Sql Qml Quick QuickControls2 LinguistTools)

qt_standard_project_setup(REQUIRES 6.8)

include(cmake/AudioDependencies.cmake)

add_library(tonal_companion STATIC src/playback/DiscoveryLyrics.hpp src/playback/DiscoveryLyrics.cpp src/companion/MusicCompanion.h src/companion/MusicCompanion.cpp src/companion/PersonalDatabase.h src/companion/PersonalDatabase.cpp src/insights/TinyMusicModel.cpp src/companion/PersonalInsights.cpp)
target_include_directories(tonal_companion PUBLIC src src/companion)
target_link_libraries(tonal_companion PUBLIC Qt6::Core Qt6::Network Qt6::Sql)
if(MSVC)
    target_compile_options(tonal_companion PRIVATE /utf-8 /W4)
endif()
add_library(tonal_playlist_core STATIC src/playlist/TextPlaylistCore.cpp src/playback/ListeningMixCore.cpp)
target_include_directories(tonal_playlist_core PUBLIC src)
add_library(tonal_text_playlist STATIC src/playlist/TextPlaylistImport.hpp src/playlist/TextPlaylistImport.cpp)
target_link_libraries(tonal_text_playlist PUBLIC tonal_playlist_core tonal_companion Qt6::Gui)
if(MSVC)
    target_compile_options(tonal_playlist_core PRIVATE /utf-8 /W4)
    target_compile_options(tonal_text_playlist PRIVATE /utf-8 /W4)
endif()

set(TONAL_SOURCES
    src/main.cpp
    src/App.h               src/App.cpp
    src/playback/ListeningMix.hpp src/playback/ListeningMix.cpp
    src/AuthManager.h       src/AuthManager.cpp
    src/TokenStore.h        src/TokenStore.cpp
    src/SpotifyApi.h        src/SpotifyApi.cpp
    src/LibrespotProcess.h  src/LibrespotProcess.cpp
    src/Player.h            src/Player.cpp
    src/MediaControls.h     src/MediaControls.cpp
    src/Lyrics.h            src/Lyrics.cpp
    src/SetlistFm.h         src/SetlistFm.cpp
    src/NetworkFactory.h    src/NetworkFactory.cpp
)

qt_add_executable(Tonal ${TONAL_SOURCES})
if(TONAL_ADVANCED_AUDIO)
    target_link_libraries(Tonal PRIVATE tonal_audio)
    target_compile_definitions(Tonal PRIVATE TONAL_ADVANCED_AUDIO=1)
    tonal_deploy_audio(Tonal)
endif()

# O registro gerado pelo qmltyperegistrar faz #include <App.h>: src/ precisa estar no include path.
target_include_directories(Tonal PRIVATE src)

# Ícone e versão do executável. Fica fora do TONAL_SOURCES: o /W4 de lá iria para o rc.exe.
if(WIN32)
    target_sources(Tonal PRIVATE resources/tonal.rc)
    set_source_files_properties(resources/tonal.rc PROPERTIES COMPILE_DEFINITIONS
        "TONAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};TONAL_VERSION_MINOR=${PROJECT_VERSION_MINOR};TONAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}")
    # Ativação das classes do Windows Runtime (controle de mídia do Windows, em MediaControls.cpp).
    target_link_libraries(Tonal PRIVATE runtimeobject)
endif()

set_source_files_properties(qml/Theme.qml qml/Icons.qml qml/Nav.qml qml/Playlists.qml qml/Setlists.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Tonal
    URI Tonal
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icons.qml
        qml/Nav.qml
        qml/Playlists.qml
        qml/Setlists.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/PlayButton.qml
        qml/components/Cover.qml
        qml/components/MediaCard.qml
        qml/components/ShortcutTile.qml
        qml/components/TrackRow.qml
        qml/components/SeekBar.qml
        qml/components/Chip.qml
        qml/components/SectionHeader.qml
        qml/components/Logo.qml
        qml/components/LikedArt.qml
        qml/components/ThinScrollBar.qml
        qml/components/HorizontalShelf.qml
        qml/components/PillButton.qml
        qml/components/ModalDialog.qml
        qml/components/DarkMenu.qml
        qml/components/DarkMenuItem.qml
        qml/panels/TopBar.qml
        qml/panels/AudioEffectsPanel.qml
        qml/panels/LibraryPanel.qml
        qml/panels/PlayerBar.qml
        qml/panels/RightPanel.qml
        qml/panels/DevicePopup.qml
        qml/pages/LoginPage.qml
        qml/pages/HomePage.qml
        qml/pages/SearchPage.qml
        qml/pages/CollectionPage.qml
        qml/pages/ArtistPage.qml
        qml/pages/LyricsPage.qml
        qml/pages/SetlistsPage.qml
        qml/pages/TextPlaylistDialog.qml
)

target_link_libraries(Tonal PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

# Separate native programs share the public-source catalog, never Spotify tokens.
add_library(tonal_indie_catalog STATIC
    src/indie/CatalogService.h src/indie/CatalogService.cpp
    src/TokenStore.cpp)
add_library(tonal_hub_launcher STATIC src/indie/HubController.h src/indie/HubController.cpp)
target_include_directories(tonal_hub_launcher PUBLIC src src/indie)
target_link_libraries(tonal_hub_launcher PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
target_include_directories(tonal_indie_catalog PUBLIC src src/indie)
target_link_libraries(tonal_indie_catalog PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
if(MSVC)
    target_compile_options(tonal_indie_catalog PRIVATE /utf-8 /W4)
    target_compile_options(tonal_hub_launcher PRIVATE /utf-8 /W4)
endif()
target_link_libraries(Tonal PRIVATE tonal_indie_catalog tonal_hub_launcher)
target_link_libraries(Tonal PRIVATE tonal_companion)
target_link_libraries(Tonal PRIVATE tonal_text_playlist)
qt_add_resources(Tonal "companion_ui" PREFIX "/companion" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES qml/companion/MusicPanel.qml qml/companion/ActionIcon.qml qml/companion/LyricBackdrop.qml qml/companion/StyledVerse.qml qml/companion/LyricStyleDialog.qml qml/companion/LyricsFullscreen.qml qml/companion/StatisticsDetail.qml qml/companion/ClimateDetail.qml resources/music-rules.json)
foreach(program TonalHub IndieTonal)
    if(program STREQUAL "TonalHub")
        qt_add_executable(${program} src/indie/hub_main.cpp)
        target_link_libraries(${program} PRIVATE tonal_hub_launcher Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    else()
        qt_add_executable(${program} src/indie/main.cpp)
        target_link_libraries(${program} PRIVATE tonal_indie_catalog tonal_hub_launcher tonal_audio tonal_companion Qt6::Qml Qt6::Quick Qt6::QuickControls2)
        qt_add_resources(${program} "companion_ui" PREFIX "/companion" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES qml/companion/MusicPanel.qml qml/companion/ActionIcon.qml qml/companion/LyricBackdrop.qml qml/companion/StyledVerse.qml qml/companion/LyricStyleDialog.qml qml/companion/LyricsFullscreen.qml qml/companion/StatisticsDetail.qml qml/companion/ClimateDetail.qml resources/music-rules.json)
        tonal_deploy_audio(${program})
    endif()
    qt_add_resources(${program} "indie_ui" PREFIX "/indie" BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml/indie" FILES qml/indie/IndieMain.qml qml/indie/HubMain.qml)
    set_target_properties(${program} PROPERTIES WIN32_EXECUTABLE TRUE)
    if(WIN32)
        target_sources(${program} PRIVATE resources/hub.rc)
    endif()
    # Dynamic Qt plugins are deployed by windeployqt. Avoid scanning the huge
    # dependency/build cache under the repository root during configuration.
    set_target_properties(${program} PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    if(MSVC)
        target_compile_options(${program} PRIVATE /utf-8)
    endif()
endforeach()

target_compile_definitions(Tonal PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")

set_target_properties(Tonal PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(MSVC)
    target_compile_options(Tonal PRIVATE /permissive- /utf-8)
    # /W4 só no código escrito à mão. O C++ que o qmlcachegen gera a partir do QML instancia
    # templates do Qt (qjsengine.h, qvariant.h) que disparam C4702 no MSVC, fora do nosso controle.
    set_source_files_properties(${TONAL_SOURCES} PROPERTIES COMPILE_OPTIONS /W4)
endif()

# No Windows, copia as DLLs do Qt para a pasta do executável depois de compilar,
# para dar F5 no Visual Studio sem configurar PATH.
option(TONAL_AUTO_DEPLOY "Rodar windeployqt depois do build" ON)
if(WIN32 AND TONAL_AUTO_DEPLOY)
    if(TARGET Qt6::windeployqt)
        set(TONAL_WINDEPLOYQT "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        find_program(TONAL_WINDEPLOYQT_EXE windeployqt
            HINTS "${QT6_INSTALL_PREFIX}/bin" "$ENV{QTDIR}/bin")
        set(TONAL_WINDEPLOYQT "${TONAL_WINDEPLOYQT_EXE}")
    endif()
    if(TONAL_WINDEPLOYQT)
        add_custom_command(TARGET Tonal POST_BUILD
            COMMAND "${TONAL_WINDEPLOYQT}"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:Tonal>"
            COMMENT "windeployqt: copiando DLLs do Qt"
            VERBATIM)
    else()
        message(WARNING "windeployqt não encontrado: rode-o manualmente para copiar as DLLs do Qt.")
    endif()
endif()

# Interface em inglês: os textos do código (qsTr/tr) são em português. "cmake --build ... --target
# update_translations" extrai os textos novos para i18n/tonal_en.ts (traduza os que ficarem "unfinished"); o
# build compila o .ts e embute em :/i18n/tonal_en.qm.
qt_add_translations(Tonal
    SOURCE_TARGETS Tonal tonal_audio
    TS_FILES i18n/tonal_en.ts
    RESOURCE_PREFIX /i18n
    NO_GENERATE_PLURALS_TS_FILE
    LUPDATE_OPTIONS -no-obsolete -locations none -source-language pt_BR -target-language en
)

include(GNUInstallDirs)
include(CTest)
get_target_property(TONAL_QT_CORE_PATH Qt6::Core IMPORTED_LOCATION_RELEASE)
get_filename_component(TONAL_QT_BIN "${TONAL_QT_CORE_PATH}" DIRECTORY)
get_filename_component(TONAL_QT_ROOT "${TONAL_QT_BIN}/.." ABSOLUTE)
if(BUILD_TESTING)
    add_executable(TonalMixChecks tests/mix/CoreChecks.cpp)
    target_link_libraries(TonalMixChecks PRIVATE tonal_playlist_core)
    add_test(NAME listening_mix_core COMMAND TonalMixChecks)
    add_executable(TonalTextPlaylistTests tests/playlist/ImportChecks.cpp)
    target_link_libraries(TonalTextPlaylistTests PRIVATE tonal_text_playlist)
    if(MSVC)
        target_compile_options(TonalTextPlaylistTests PRIVATE /utf-8)
    endif()
    add_test(NAME text_playlist_import COMMAND TonalTextPlaylistTests)
    add_executable(TonalCompanionTests tests/companion/CompanionTests.cpp)
    target_link_libraries(TonalCompanionTests PRIVATE tonal_companion)
    qt_add_resources(TonalCompanionTests "companion_rules" PREFIX "/companion" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES resources/music-rules.json)
    if(MSVC)
        target_compile_options(TonalCompanionTests PRIVATE /utf-8)
    endif()
    add_test(NAME companion_unit COMMAND TonalCompanionTests)
    add_test(NAME companion_ui_smoke COMMAND IndieTonal --smoke-test --panel music)
    add_executable(TonalIndieTests tests/indie/CatalogTests.cpp)
    target_link_libraries(TonalIndieTests PRIVATE tonal_indie_catalog tonal_audio)
    if(MSVC)
        target_compile_options(TonalIndieTests PRIVATE /utf-8)
    endif()
    add_test(NAME indie_catalog_unit COMMAND TonalIndieTests)
    add_test(NAME indie_ui_smoke COMMAND IndieTonal --smoke-test)
    add_test(NAME hub_ui_smoke COMMAND TonalHub --smoke-test)
    set_tests_properties(companion_ui_smoke indie_ui_smoke hub_ui_smoke PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ALSOFT_DRIVERS=null;QT_PLUGIN_PATH=${TONAL_QT_ROOT}/plugins;QML_IMPORT_PATH=${TONAL_QT_ROOT}/qml;QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts;QT_FORCE_STDERR_LOGGING=1")
endif()
if(BUILD_TESTING AND TONAL_ADVANCED_AUDIO)
    add_executable(TonalSpotifyRecoveryTests tests/spotify/RecoveryTests.cpp
        src/LibrespotProcess.cpp src/Player.cpp src/SpotifyApi.cpp src/AuthManager.cpp src/playback/ListeningMix.cpp src/playback/ListeningMix.hpp)
    target_include_directories(TonalSpotifyRecoveryTests PRIVATE src)
    target_link_libraries(TonalSpotifyRecoveryTests PRIVATE tonal_audio tonal_indie_catalog tonal_companion tonal_playlist_core Qt6::Qml)
    qt_add_resources(TonalSpotifyRecoveryTests "mix_rules" PREFIX "/companion" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES resources/music-rules.json)
    if(MSVC)
        target_compile_options(TonalSpotifyRecoveryTests PRIVATE /utf-8 /W4)
    endif()
    tonal_deploy_audio(TonalSpotifyRecoveryTests)
    add_test(NAME spotify_recovery COMMAND TonalSpotifyRecoveryTests --local-only --report "${CMAKE_SOURCE_DIR}/out/research/spotify-recovery-tests.json")
    set_tests_properties(spotify_recovery PROPERTIES TIMEOUT 45 ENVIRONMENT "ALSOFT_DRIVERS=null")
    add_library(tonal_iamf_fallback_compile OBJECT src/audio/IamfDecoderAdapter.cpp)
    target_link_libraries(tonal_iamf_fallback_compile PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(tonal_iamf_fallback_compile PRIVATE /utf-8)
    endif()
    add_executable(TonalAudioTests tests/audio/AudioTests.cpp)
    target_link_libraries(TonalAudioTests PRIVATE tonal_audio)
    if(MSVC)
        target_compile_options(TonalAudioTests PRIVATE /utf-8)
    endif()
    tonal_deploy_audio(TonalAudioTests)
    add_test(NAME audio_integration COMMAND TonalAudioTests "${CMAKE_SOURCE_DIR}/out/audio-fixtures")
    set_tests_properties(audio_integration PROPERTIES TIMEOUT 90)
    add_test(NAME audio_ui_smoke COMMAND Tonal --audio-smoke-test --local-only --audio-settings --local-file "${CMAKE_SOURCE_DIR}/out/audio-fixtures/stereo-ui-48000.wav")
    get_target_property(TONAL_QT_CORE_PATH Qt6::Core IMPORTED_LOCATION_RELEASE)
    get_filename_component(TONAL_QT_BIN "${TONAL_QT_CORE_PATH}" DIRECTORY)
    get_filename_component(TONAL_QT_ROOT "${TONAL_QT_BIN}/.." ABSOLUTE)
    set_tests_properties(audio_ui_smoke PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ALSOFT_DRIVERS=null;QT_PLUGIN_PATH=${TONAL_QT_ROOT}/plugins;QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts;QT_FORCE_STDERR_LOGGING=1")
endif()
install(TARGETS Tonal
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
