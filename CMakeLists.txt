cmake_minimum_required(VERSION 3.21)

project(KeyboardEDO VERSION 0.1.0 LANGUAGES CXX)

# KeyboardEDO: a cross-platform, UMP-first EDO keyboard controller, after KeyEDO
# (~/Dev-MIDI/KeyEDO). docs/decisions.md says why it's built this way and
# docs/roadmap.md what comes next.
#
#   kedo::core   tuning, layouts, the playing logic and UMP output; no toolkit
#   keyboardedo  the app: SDL3 and Dear ImGui (docs/decisions.md, "UI")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KEDO_TESTS "Build KeyboardEDO's tests" ${PROJECT_IS_TOP_LEVEL})
option(KEDO_APP "Build the app (needs SDL3's platform dependencies)" ON)
option(KEDO_SYSTEM_SDL "Use an installed SDL3 instead of external/SDL" OFF)

# uw-core, a git submodule pinned to a commit. The core needs only its UMP
# layer; the app adds uw::midi, its MIDI output (CoreMIDI, or the ALSA
# sequencer on Linux). Building uw::midi also builds uw::base, uw::audio and
# the CLAP host, which need uw-core's own submodule
# (git submodule update --init --recursive), and on Linux JACK's and ALSA's
# development packages.
if(KEDO_APP)
    set(UW_CORE_UMP_ONLY OFF CACHE BOOL "" FORCE)
else()
    set(UW_CORE_UMP_ONLY ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(external/uw-core)

add_library(kedo-warnings INTERFACE)
target_compile_options(kedo-warnings INTERFACE -Wall -Wextra -Wpedantic)

add_library(kedo-core STATIC
    src/kedo/Tuning.cpp
    src/kedo/Generators.cpp
    src/kedo/Layout.cpp
    src/kedo/CustomLayout.cpp
    src/kedo/ComputerKeys.cpp
    src/kedo/UmpOutput.cpp
    src/kedo/Performer.cpp
    src/kedo/Arpeggiator.cpp
    src/kedo/Controls.cpp
    src/kedo/Viewport.cpp
    src/kedo/KeyStyle.cpp
    src/kedo/UmpText.cpp
    src/kedo/Document.cpp
    src/kedo/Preferences.cpp
    src/kedo/Synopsis.cpp
)
add_library(kedo::core ALIAS kedo-core)
target_include_directories(kedo-core PUBLIC src)
target_link_libraries(kedo-core PUBLIC uw::ump PRIVATE kedo-warnings)
# nlohmann/json (a shallow submodule pinned to v3.12.0), for documents.
target_include_directories(kedo-core SYSTEM PRIVATE external/json/single_include)

# Draws every layout to SVG, to check the geometry by eye without a UI.
add_executable(kedo-layout-svg tools/LayoutSvg.cpp)
target_link_libraries(kedo-layout-svg PRIVATE kedo::core kedo-warnings)

if(KEDO_TESTS)
    enable_testing()
    foreach(t TuningTests LayoutTests UmpOutputTests PerformerTests ArpeggiatorTests ControlsTests ViewTests DocumentTests)
        add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE kedo::core kedo-warnings)
        target_include_directories(${t} SYSTEM PRIVATE external/json/single_include)
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
endif()

# --- The app ----------------------------------------------------------------------

if(KEDO_APP)
    # SDL3 and Dear ImGui are git submodules pinned to releases, as uw-core, so
    # the build doesn't depend on what the package manager has.
    if(KEDO_SYSTEM_SDL)
        find_package(SDL3 REQUIRED CONFIG)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
        set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
    endif()

    set(IMGUI_DIR external/imgui)
    add_library(kedo-imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(kedo-imgui SYSTEM PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(kedo-imgui PUBLIC SDL3::SDL3)

    include(cmake/EmbedFile.cmake)
    embed_file(${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}/misc/fonts/Cousine-Regular.ttf
               ${CMAKE_CURRENT_BINARY_DIR}/MonoFont.cpp kMonoFont)
    # The toolbar's Unlikelyware logo and wordmark font, as the plugins have
    # (resources/README.md says where they come from).
    embed_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brand/Unlikelyware-logo-144.png
               ${CMAKE_CURRENT_BINARY_DIR}/BrandLogo.cpp kBrandLogo)
    embed_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/Karla-Regular.ttf
               ${CMAKE_CURRENT_BINARY_DIR}/BrandFont.cpp kBrandFont)

    add_executable(keyboardedo
        src/app/main.cpp
        src/app/KeyboardApp.cpp
        src/app/MidiTargets.cpp
        src/app/Icons.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/MonoFont.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/BrandLogo.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/BrandFont.cpp
    )
    target_link_libraries(keyboardedo PRIVATE kedo::core kedo-imgui kedo-warnings)
    if(TARGET uw::midi)
        target_link_libraries(keyboardedo PRIVATE uw::midi)
        # Prints what a source sends: checks that notes leave the app.
        add_executable(kedo-midi-listen tools/MidiListen.cpp)
        target_link_libraries(kedo-midi-listen PRIVATE kedo::core uw::midi kedo-warnings)
    endif()

    # macOS: a KeyboardEDO.app bundle (docs/decisions.md, "Identity and help"),
    # whose icon is compiled from resources/KeyboardEDO.icon, an Icon Composer
    # document, by Xcode's actool. Without Xcode (only the Command Line Tools)
    # the bundle has no icon. build/keyboardedo is a link to the executable
    # inside, so command lines (--snapshot and the rest) stay as they were.
    if(APPLE)
        set(KEDO_MACOS_MINIMUM 11.0)
        configure_file(cmake/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
        set_target_properties(keyboardedo PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME KeyboardEDO
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

        execute_process(COMMAND xcrun --find actool OUTPUT_VARIABLE KEDO_ACTOOL
                        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE actool_missing)
        if(actool_missing)
            message(WARNING "Xcode's actool not found: KeyboardEDO.app will have no icon "
                            "(install Xcode, not just the Command Line Tools)")
        else()
            set(icon ${CMAKE_CURRENT_SOURCE_DIR}/resources/KeyboardEDO.icon)
            set(icon_out ${CMAKE_CURRENT_BINARY_DIR}/icon)
            file(GLOB_RECURSE icon_inputs CONFIGURE_DEPENDS ${icon}/*)
            add_custom_command(
                OUTPUT ${icon_out}/Assets.car ${icon_out}/KeyboardEDO.icns
                COMMAND ${CMAKE_COMMAND} -E make_directory ${icon_out}
                COMMAND ${KEDO_ACTOOL} ${icon} --compile ${icon_out} --app-icon KeyboardEDO
                        --platform macosx --target-device mac
                        --minimum-deployment-target ${KEDO_MACOS_MINIMUM}
                        --output-partial-info-plist ${icon_out}/partial.plist
                        --errors --warnings --output-format human-readable-text
                DEPENDS ${icon_inputs}
                COMMENT "Compiling the app icon"
                VERBATIM)
            set(icon_files ${icon_out}/Assets.car ${icon_out}/KeyboardEDO.icns)
            target_sources(keyboardedo PRIVATE ${icon_files})
            set_source_files_properties(${icon_files} PROPERTIES
                GENERATED TRUE MACOSX_PACKAGE_LOCATION Resources)
        endif()

        # Ad-hoc signed once whole (the linker signs only the executable), and
        # the link for the command line.
        add_custom_command(TARGET keyboardedo POST_BUILD
            COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:keyboardedo>
            COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/keyboardedo
            COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:keyboardedo> ${CMAKE_BINARY_DIR}/keyboardedo
            VERBATIM)
    endif()
endif()
