cmake_minimum_required(VERSION 3.20)

project(imuse_v6 LANGUAGES CXX)

option(IMUSE_BUILD_TOOLS "Build imuse command-line tools" ON)
option(IMUSE_BUILD_GUI_TOOLS "Build imuse Qt6 GUI tools" ON)
option(IMUSE_BUILD_SETMIDI "Build the native Windows SetMIDI utility" ON)
option(IMUSE_BUILD_AGS_PLUGIN "Build the AGS runtime plugin" ON)
option(IMUSE_BUILD_SHARED_LIB "Build the shared imuse_v6 library" ON)
option(IMUSE_BUILD_SHIM "Build the native imuse_shim DLL used by wrappers and editor tooling" ON)
option(IMUSE_SHIM_ENABLE_EXTERNAL_BACKENDS "Enable optional playback backends in imuse_shim" OFF)

set(IMUSE_QT6_PREFIX_PATH "" CACHE PATH "Optional Qt6 installation prefix")
set(IMUSE_FLUIDSYNTH_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-build" CACHE PATH "Path to the prebuilt FluidSynth build tree used by agsimuse")

set(IMUSE_CORE_SOURCES
    src/Instrument.cpp
    src/ImuseSequence.cpp
    src/ImuseSysex.cpp
    src/ImsWriter.cpp
    src/ImuseEngine.cpp
    src/ResourceBank.cpp
    src/SinkMidiChannel.cpp
    src/SmfSequence.cpp
)

function(imuse_apply_common_target_settings target_name)
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

if(IMUSE_BUILD_GUI_TOOLS)
    if(IMUSE_QT6_PREFIX_PATH)
        list(APPEND CMAKE_PREFIX_PATH "${IMUSE_QT6_PREFIX_PATH}")
    elseif(EXISTS "D:/Outils/Qt/6.8.0/msvc2022_64")
        list(APPEND CMAKE_PREFIX_PATH "D:/Outils/Qt/6.8.0/msvc2022_64")
    endif()

    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

add_library(imuse_v6 STATIC ${IMUSE_CORE_SOURCES})
set_target_properties(imuse_v6 PROPERTIES OUTPUT_NAME "imuse_v6_static")
imuse_apply_common_target_settings(imuse_v6)

if(IMUSE_BUILD_SHARED_LIB)
    add_library(imuse_v6_shared SHARED ${IMUSE_CORE_SOURCES})
    target_compile_definitions(imuse_v6_shared PRIVATE IMUSE_DLL IMUSE_BUILD_DLL)
    target_compile_definitions(imuse_v6_shared INTERFACE IMUSE_DLL)
    set_target_properties(imuse_v6_shared PROPERTIES
        OUTPUT_NAME "imuse_v6"
        ARCHIVE_OUTPUT_NAME "imuse_v6_dll"
    )
    imuse_apply_common_target_settings(imuse_v6_shared)
endif()

if(IMUSE_BUILD_TOOLS)
    add_executable(imusepack tools/imusepack.cpp)
    target_link_libraries(imusepack PRIVATE imuse_v6)
    imuse_apply_common_target_settings(imusepack)

    add_executable(imsprobe tools/imsprobe.cpp)
    target_link_libraries(imsprobe PRIVATE imuse_v6)
    imuse_apply_common_target_settings(imsprobe)
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

if(IMUSE_BUILD_SHIM)
    add_library(imuse_shim SHARED
        swift-shim/CImuseShim.cpp
    )

    target_include_directories(imuse_shim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/swift-shim/include
    )

    target_link_libraries(imuse_shim PRIVATE
        imuse_v6
    )

    set_target_properties(imuse_shim PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    if(IMUSE_SHIM_ENABLE_EXTERNAL_BACKENDS)
        target_compile_definitions(imuse_shim PRIVATE IMUSE_SHIM_WITH_ADLIB=1)
        target_include_directories(imuse_shim PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include
        )
        target_link_libraries(imuse_shim PRIVATE ADLMIDI)

        if(EXISTS "${IMUSE_FLUIDSYNTH_BUILD_DIR}/include/fluidsynth.h" AND EXISTS "${IMUSE_FLUIDSYNTH_BUILD_DIR}/src/Release/libfluidsynth-3.lib")
            target_compile_definitions(imuse_shim PRIVATE IMUSE_SHIM_WITH_FLUIDSYNTH=1)
            target_include_directories(imuse_shim PRIVATE
                ${IMUSE_FLUIDSYNTH_BUILD_DIR}/include
            )
            target_link_libraries(imuse_shim PRIVATE
                "${IMUSE_FLUIDSYNTH_BUILD_DIR}/src/$<CONFIG>/libfluidsynth-3.lib"
                winmm
                dsound
                ole32
                ksuser
            )
        endif()

        find_path(IMUSE_MT32EMU_INCLUDE_DIR
            NAMES mt32emu.h
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/include
            NO_DEFAULT_PATH
        )
        find_library(IMUSE_MT32EMU_LIBRARY
            NAMES mt32emu libmt32emu
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/Release
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/lib
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mt32emu-build/lib/Release
            NO_DEFAULT_PATH
        )

        if(IMUSE_MT32EMU_INCLUDE_DIR AND IMUSE_MT32EMU_LIBRARY)
            target_compile_definitions(imuse_shim PRIVATE IMUSE_SHIM_WITH_MT32EMU=1)
            target_include_directories(imuse_shim PRIVATE
                ${IMUSE_MT32EMU_INCLUDE_DIR}
            )
            target_link_libraries(imuse_shim PRIVATE
                ${IMUSE_MT32EMU_LIBRARY}
            )
        endif()
    endif()

    imuse_apply_common_target_settings(imuse_shim)
endif()

if(IMUSE_BUILD_AGS_PLUGIN)
    if(NOT EXISTS "${IMUSE_FLUIDSYNTH_BUILD_DIR}/include/fluidsynth.h")
        message(FATAL_ERROR "IMUSE_BUILD_AGS_PLUGIN=ON requires a prebuilt FluidSynth tree with headers under ${IMUSE_FLUIDSYNTH_BUILD_DIR}/include")
    endif()

    add_library(agsimuse SHARED
        src/AgsImusePlugin.cpp
    )

    target_compile_definitions(agsimuse PRIVATE THIS_IS_THE_PLUGIN)

    target_include_directories(agsimuse PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ags
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-master/include
        ${IMUSE_FLUIDSYNTH_BUILD_DIR}/include
    )

    target_link_libraries(agsimuse PRIVATE
        imuse_v6
        ADLMIDI
        "${IMUSE_FLUIDSYNTH_BUILD_DIR}/src/$<CONFIG>/libfluidsynth-3.lib"
        winmm
        dsound
        ole32
        ksuser
    )

    imuse_apply_common_target_settings(agsimuse)

    if(MSVC)
        target_link_libraries(agsimuse PRIVATE winmm)
    else()
        if(NOT APPLE)
            target_link_libraries(agsimuse PRIVATE winmm)
        endif()
    endif()

    if(APPLE)
        target_link_libraries(agsimuse PRIVATE
            "-framework CoreAudio"
            "-framework AudioUnit"
            "-framework AudioToolbox"
            "-framework CoreMIDI"
            "-framework CoreFoundation"
            m
        )
    endif()
endif()

if(IMUSE_BUILD_GUI_TOOLS)
    # SysEx Tool
    add_executable(imuse_sysex_gui WIN32 tools_gui/sysex/main.cpp tools_gui/sysex/SysExWindow.cpp)
    target_link_libraries(imuse_sysex_gui PRIVATE Qt6::Widgets imuse_v6 winmm)
    target_include_directories(imuse_sysex_gui PRIVATE tools_gui/sysex)

    # Player Tool
    add_executable(imuse_player_gui WIN32 tools_gui/player/main.cpp tools_gui/player/PlayerWindow.cpp)
    target_link_libraries(imuse_player_gui PRIVATE Qt6::Widgets imuse_v6 winmm dsound ole32 ksuser)
    target_include_directories(imuse_player_gui PRIVATE tools_gui/player)

    # Packer Tool
    add_executable(imuse_packer_gui WIN32 tools_gui/packer/main.cpp tools_gui/packer/PackerWindow.cpp)
    target_link_libraries(imuse_packer_gui PRIVATE Qt6::Widgets imuse_v6)
    target_include_directories(imuse_packer_gui PRIVATE tools_gui/packer)

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

            add_custom_command(TARGET imuse_sysex_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imuse_sysex_gui>\"
            )
            add_custom_command(TARGET imuse_player_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imuse_player_gui>\"
            )
            add_custom_command(TARGET imuse_packer_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${MINIMAL_WINDEPLOYQT_ARGS} \"$<TARGET_FILE:imuse_packer_gui>\"
            )
        endif()
    endif()
endif()

if(WIN32 AND IMUSE_BUILD_SETMIDI)
    add_executable(imuse_setmidi WIN32 tools_gui/setmidi/main.cpp tools_gui/setmidi/MidiConfigWindow.cpp)
    target_link_libraries(imuse_setmidi PRIVATE winmm comctl32)
    target_include_directories(imuse_setmidi PRIVATE tools_gui/setmidi)
    set_target_properties(imuse_setmidi PROPERTIES
        OUTPUT_NAME "SetMIDI"
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
    )
    imuse_apply_common_target_settings(imuse_setmidi)

    if(MSVC)
        set_target_properties(imuse_setmidi PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()
