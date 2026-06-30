cmake_minimum_required(VERSION 3.20)

project(imwrap_v6 LANGUAGES CXX)

option(IMWRAP_BUILD_TOOLS "Build imwrap command-line tools" ON)
option(IMWRAP_BUILD_GUI_TOOLS "Build imwrap Qt6 GUI tools" ON)
option(IMWRAP_BUILD_SETMIDI "Build the native Windows SetMIDI utility" ON)
option(IMWRAP_BUILD_AGS_PLUGIN "Build the AGS runtime plugin" ON)
option(IMWRAP_BUILD_SHARED_LIB "Build the shared imwrap_v6 library" ON)
option(IMWRAP_BUILD_SHIM "Build the native imwrap_shim DLL used by wrappers and editor tooling" ON)
option(IMWRAP_BUILD_VST3_TOOL "Build the VST3 SysEx generator proof of concept" OFF)
option(IMWRAP_SHIM_ENABLE_EXTERNAL_BACKENDS "Enable optional playback backends in imwrap_shim" OFF)

set(IMWRAP_QT6_PREFIX_PATH "" CACHE PATH "Optional Qt6 installation prefix")
set(IMWRAP_FLUIDSYNTH_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-build" CACHE PATH "Path to the prebuilt FluidSynth build tree used by agsimwrap")
set(IMWRAP_VST3_SDK_DIR "" CACHE PATH "Path to a local Steinberg VST3 SDK checkout")

# The Win32 AGS plugin release is built without Qt and links against prebuilt
# static third-party libraries, so align the whole configuration on the static
# MSVC runtime to avoid CRT mismatches. Qt-enabled builds keep the default CRT.
if(MSVC AND IMWRAP_BUILD_AGS_PLUGIN AND NOT IMWRAP_BUILD_GUI_TOOLS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(IMWRAP_CORE_SOURCES
    src/Instrument.cpp
    src/IMWrapSequence.cpp
    src/IMWrapSysex.cpp
    src/ImsWriter.cpp
    src/IMWrapEngine.cpp
    src/IMWrapEngineSerialize.cpp
    src/ResourceBank.cpp
    src/ScummAdlibSink.cpp
    src/SinkMidiChannel.cpp
    src/SmfSequence.cpp
)

function(imwrap_apply_common_target_settings target_name)
    target_include_directories(${target_name}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(${target_name} PUBLIC cxx_std_17)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(IMWRAP_BUILD_GUI_TOOLS)
    if(IMWRAP_QT6_PREFIX_PATH)
        list(APPEND CMAKE_PREFIX_PATH "${IMWRAP_QT6_PREFIX_PATH}")
    endif()

    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

add_library(imwrap_v6 STATIC ${IMWRAP_CORE_SOURCES})
set_target_properties(imwrap_v6 PROPERTIES OUTPUT_NAME "imwrap_v6_static")
imwrap_apply_common_target_settings(imwrap_v6)

if(IMWRAP_BUILD_SHARED_LIB)
    add_library(imwrap_v6_shared SHARED
        ${IMWRAP_CORE_SOURCES}
        tools_gui/shared-res/imwrap_icon_bundle.rc
    )
    target_compile_definitions(imwrap_v6_shared PRIVATE IMWRAP_DLL IMWRAP_BUILD_DLL)
    target_compile_definitions(imwrap_v6_shared INTERFACE IMWRAP_DLL)
    set_target_properties(imwrap_v6_shared PROPERTIES
        OUTPUT_NAME "imwrap_v6"
        ARCHIVE_OUTPUT_NAME "imwrap_v6_dll"
    )
    imwrap_apply_common_target_settings(imwrap_v6_shared)
endif()

if(IMWRAP_BUILD_TOOLS)
    add_executable(imwrappack
        tools/imwrappack.cpp
        tools_gui/shared-res/imwrappack.rc
    )
    target_link_libraries(imwrappack PRIVATE imwrap_v6)
    imwrap_apply_common_target_settings(imwrappack)

    add_executable(imsprobe
        tools/imsprobe.cpp
        tools_gui/shared-res/imsprobe.rc
    )
    target_link_libraries(imsprobe PRIVATE imwrap_v6)
    imwrap_apply_common_target_settings(imsprobe)
endif()

# Configure libADLMIDI build options
set(libADLMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libADLMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(WITH_MIDIPLAY OFF CACHE BOOL "" FORCE)
set(WITH_ADLMIDI2 OFF CACHE BOOL "" FORCE)
set(WITH_MUS2MID OFF CACHE BOOL "" FORCE)
set(WITH_XMI2MID OFF CACHE BOOL "" FORCE)
set(WITH_MIDIDUMP OFF CACHE BOOL "" FORCE)
set(EXAMPLE_SDL2_AUDIO OFF CACHE BOOL "" FORCE)
set(WITH_MIDI_SEQUENCER ON CACHE BOOL "" FORCE)

add_subdirectory(third_party/libadlmidi)

if(IMWRAP_BUILD_SHARED_LIB)
    target_link_libraries(imwrap_v6_shared PRIVATE ADLMIDI)
endif()

if(IMWRAP_BUILD_SHIM)
    add_library(imwrap_shim SHARED
        swift-shim/CIMWrapShim.cpp
    )

    target_include_directories(imwrap_shim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/swift-shim/include
    )

    target_link_libraries(imwrap_shim PRIVATE
        imwrap_v6
    )

    set_target_properties(imwrap_shim PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    if(IMWRAP_SHIM_ENABLE_EXTERNAL_BACKENDS)
        target_compile_definitions(imwrap_shim PRIVATE IMWRAP_SHIM_WITH_ADLIB=1)
        target_include_directories(imwrap_shim PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include
        )
        target_link_libraries(imwrap_shim PRIVATE ADLMIDI)

        if(EXISTS "${IMWRAP_FLUIDSYNTH_BUILD_DIR}/include/fluidsynth.h" AND EXISTS "${IMWRAP_FLUIDSYNTH_BUILD_DIR}/src/Release/libfluidsynth-3.lib")
            target_compile_definitions(imwrap_shim PRIVATE IMWRAP_SHIM_WITH_FLUIDSYNTH=1)
            target_include_directories(imwrap_shim PRIVATE
                ${IMWRAP_FLUIDSYNTH_BUILD_DIR}/include
            )
            target_link_libraries(imwrap_shim PRIVATE
                "${IMWRAP_FLUIDSYNTH_BUILD_DIR}/src/$<CONFIG>/libfluidsynth-3.lib"
                winmm
                dsound
                ole32
                ksuser
            )
        endif()

        find_path(IMWRAP_MT32EMU_INCLUDE_DIR
            NAMES mt32emu.h
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/include
            NO_DEFAULT_PATH
        )
        find_library(IMWRAP_MT32EMU_LIBRARY
            NAMES mt32emu libmt32emu
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/Release
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/lib
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/lib/Release
            NO_DEFAULT_PATH
        )

        if(IMWRAP_MT32EMU_INCLUDE_DIR AND IMWRAP_MT32EMU_LIBRARY)
            target_compile_definitions(imwrap_shim PRIVATE IMWRAP_SHIM_WITH_MT32EMU=1)
            target_include_directories(imwrap_shim PRIVATE
                ${IMWRAP_MT32EMU_INCLUDE_DIR}
            )
            target_link_libraries(imwrap_shim PRIVATE
                ${IMWRAP_MT32EMU_LIBRARY}
            )
        endif()
    endif()

    imwrap_apply_common_target_settings(imwrap_shim)
endif()

if(IMWRAP_BUILD_AGS_PLUGIN)
    if(NOT EXISTS "${IMWRAP_FLUIDSYNTH_BUILD_DIR}/include/fluidsynth.h")
        message(FATAL_ERROR "IMWRAP_BUILD_AGS_PLUGIN=ON requires a prebuilt FluidSynth tree with headers under ${IMWRAP_FLUIDSYNTH_BUILD_DIR}/include")
    endif()

    add_library(agsimwrap SHARED
        src/AgsIMWrapPlugin.cpp
        tools_gui/shared-res/imwrap_icon_bundle.rc
    )

    target_compile_definitions(agsimwrap PRIVATE THIS_IS_THE_PLUGIN)

    target_include_directories(agsimwrap PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ags
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-master/include
        ${IMWRAP_FLUIDSYNTH_BUILD_DIR}/include
    )

    target_link_libraries(agsimwrap PRIVATE
        imwrap_v6
        ADLMIDI
        "${IMWRAP_FLUIDSYNTH_BUILD_DIR}/src/$<CONFIG>/libfluidsynth-3.lib"
        winmm
        dsound
        ole32
        ksuser
    )

    imwrap_apply_common_target_settings(agsimwrap)

    if(MSVC)
        target_link_libraries(agsimwrap PRIVATE winmm)
    else()
        if(NOT APPLE)
            target_link_libraries(agsimwrap PRIVATE winmm)
        endif()
    endif()

    if(APPLE)
        target_link_libraries(agsimwrap PRIVATE
            "-framework CoreAudio"
            "-framework AudioUnit"
            "-framework AudioToolbox"
            "-framework CoreMIDI"
            "-framework CoreFoundation"
            m
        )
    endif()
endif()

if(IMWRAP_BUILD_GUI_TOOLS)
    # SysEx Tool
    add_executable(imwrap_sysex_gui WIN32
        tools_gui/sysex/main.cpp
        tools_gui/sysex/SysExWindow.cpp
        tools_gui/common/OplSbi.cpp
        tools_gui/shared-res/imwrap_gui_resources.qrc
        tools_gui/shared-res/imwrap_sysex_gui.rc
    )
    target_link_libraries(imwrap_sysex_gui PRIVATE Qt6::Widgets imwrap_v6 winmm)
    target_include_directories(imwrap_sysex_gui PRIVATE tools_gui/sysex tools_gui/common)

    # Player Tool
    add_executable(imwrap_player_gui WIN32
        tools_gui/player/main.cpp
        tools_gui/player/PlayerWindow.cpp
        tools_gui/common/AdlibPreviewOutput.cpp
        tools_gui/shared-res/imwrap_gui_resources.qrc
        tools_gui/shared-res/imwrap_player_gui.rc
    )
    target_link_libraries(imwrap_player_gui PRIVATE Qt6::Widgets imwrap_v6 ADLMIDI winmm dsound ole32 ksuser)
    target_include_directories(imwrap_player_gui PRIVATE tools_gui/player tools_gui/common)

    # Packer Tool
    add_executable(imwrap_packer_gui WIN32
        tools_gui/packer/main.cpp
        tools_gui/packer/PackerWindow.cpp
        tools_gui/shared-res/imwrap_gui_resources.qrc
        tools_gui/shared-res/imwrap_packer_gui.rc
    )
    target_link_libraries(imwrap_packer_gui PRIVATE Qt6::Widgets imwrap_v6)
    target_include_directories(imwrap_packer_gui PRIVATE tools_gui/packer)

    # Explorer Tool
    add_executable(imwrap_explorer_gui WIN32
        tools_gui/explorer/main.cpp
        tools_gui/explorer/ExplorerWindow.cpp
        tools_gui/explorer/IMWrapSysExDialog.cpp
        tools_gui/common/AdlibPreviewOutput.cpp
        tools_gui/common/ImsProject.cpp
        tools_gui/common/OplSbi.cpp
        tools_gui/shared-res/imwrap_gui_resources.qrc
        tools_gui/shared-res/imwrap_explorer_gui.rc
    )
    target_link_libraries(imwrap_explorer_gui PRIVATE Qt6::Widgets imwrap_v6 ADLMIDI winmm dsound ole32 ksuser)
    target_include_directories(imwrap_explorer_gui PRIVATE tools_gui/explorer tools_gui/common)

    # Windeployqt to copy required DLLs and plugins
    if(MSVC OR MINGW)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
        if(WINDEPLOYQT_EXECUTABLE)
            set(MINIMAL_WINDEPLOYQT_ARGS
                --no-network
                --no-svg
                --no-opengl-sw
                --no-system-d3d-compiler
                --no-system-dxc-compiler
                --no-translations
                --skip-plugin-types imageformats,iconengines,networkinformation,tls
            )

            add_custom_command(TARGET imwrap_sysex_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imwrap_sysex_gui>\"
            )
            add_custom_command(TARGET imwrap_player_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imwrap_player_gui>\"
            )
            add_custom_command(TARGET imwrap_packer_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imwrap_packer_gui>\"
            )
            add_custom_command(TARGET imwrap_explorer_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imwrap_explorer_gui>\"
            )
        endif()
    endif()
endif()

if(WIN32 AND IMWRAP_BUILD_SETMIDI)
    add_executable(imwrap_setmidi WIN32
        tools_gui/setmidi/main.cpp
        tools_gui/setmidi/MidiConfigWindow.cpp
        tools_gui/shared-res/imwrap_setmidi.rc
    )
    target_link_libraries(imwrap_setmidi PRIVATE winmm comctl32)
    target_include_directories(imwrap_setmidi PRIVATE tools_gui/setmidi tools_gui/shared-res)
    set_target_properties(imwrap_setmidi PROPERTIES
        OUTPUT_NAME "SetMIDI"
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
    )
    imwrap_apply_common_target_settings(imwrap_setmidi)

    if(MSVC)
        set_target_properties(imwrap_setmidi PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

if(IMWRAP_BUILD_VST3_TOOL)
    if(CMAKE_VERSION VERSION_LESS 3.25)
        message(FATAL_ERROR "IMWRAP_BUILD_VST3_TOOL=ON requires CMake 3.25 or newer because the Steinberg VST3 SDK requires it.")
    endif()

    if(NOT IMWRAP_VST3_SDK_DIR)
        message(FATAL_ERROR "IMWRAP_BUILD_VST3_TOOL=ON requires IMWRAP_VST3_SDK_DIR to point to a local Steinberg VST3 SDK checkout.")
    endif()

    get_filename_component(IMWRAP_VST3_SDK_DIR "${IMWRAP_VST3_SDK_DIR}" ABSOLUTE)
    if(NOT EXISTS "${IMWRAP_VST3_SDK_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "IMWRAP_VST3_SDK_DIR does not look like a VST3 SDK checkout: ${IMWRAP_VST3_SDK_DIR}")
    endif()

    # Reuse the SDK helper targets without building its examples or VSTGUI.
    set(SDK_ROOT "${IMWRAP_VST3_SDK_DIR}")
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)

    add_subdirectory("${IMWRAP_VST3_SDK_DIR}" "${CMAKE_BINARY_DIR}/third_party/vst3sdk" EXCLUDE_FROM_ALL)
    add_subdirectory(sysex-tool-vst)
endif()
