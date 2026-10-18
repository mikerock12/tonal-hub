cmake_minimum_required(VERSION 3.21)
project(TonalPersonalChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Sql)
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
target_link_libraries(TonalPlayerCompileCheck PRIVATE Qt6::Core Qt6::Network)
if(MSVC)
    target_compile_options(TonalCompanionChecks PRIVATE /utf-8)
    target_compile_options(TonalPlayerCompileCheck PRIVATE /utf-8)
endif()
qt_add_resources(TonalCompanionChecks "rules" PREFIX "/companion" BASE "${ROOT}" FILES "${ROOT}/resources/music-rules.json")
enable_testing()
add_test(NAME personal_library COMMAND TonalCompanionChecks)
