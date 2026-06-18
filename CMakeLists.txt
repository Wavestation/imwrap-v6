cmake_minimum_required(VERSION 3.20)

project(imuse_v6 LANGUAGES CXX)

option(IMUSE_BUILD_TOOLS "Build imuse command-line tools" ON)
option(IMUSE_BUILD_GUI_TOOLS "Build imuse Qt6 GUI tools" ON)

if(IMUSE_BUILD_GUI_TOOLS)
    list(APPEND CMAKE_PREFIX_PATH "D:/Outils/Qt/6.8.0/msvc2022_64")
    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

add_library(imuse_v6 STATIC
    src/Instrument.cpp
    src/ImuseSequence.cpp
    src/ImuseSysex.cpp
    src/ImsWriter.cpp
    src/ImuseEngine.cpp
    src/ResourceBank.cpp
    src/SinkMidiChannel.cpp
    src/SmfSequence.cpp
)

target_include_directories(imuse_v6
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(imuse_v6 PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(imuse_v6 PRIVATE /W4)
else()
    target_compile_options(imuse_v6 PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(IMUSE_BUILD_TOOLS)
    add_executable(imusepack tools/imusepack.cpp)
    target_link_libraries(imusepack PRIVATE imuse_v6)
    target_compile_features(imusepack PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(imusepack PRIVATE /W4)
    else()
        target_compile_options(imusepack PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(imsprobe tools/imsprobe.cpp)
    target_link_libraries(imsprobe PRIVATE imuse_v6)
    target_compile_features(imsprobe PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(imsprobe PRIVATE /W4)
    else()
        target_compile_options(imsprobe PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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

add_library(agsimuse SHARED
    src/AgsImusePlugin.cpp
)

target_compile_definitions(agsimuse PRIVATE THIS_IS_THE_PLUGIN)

target_include_directories(agsimuse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ags
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libadlmidi/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-master/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-build/include
)

target_link_libraries(agsimuse PRIVATE
    imuse_v6
    ADLMIDI
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fluidsynth-build/src/Release/libfluidsynth-3.lib
    winmm
    dsound
    ole32
    ksuser
)

target_compile_features(agsimuse PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(agsimuse PRIVATE /W4)
    target_link_libraries(agsimuse PRIVATE winmm)
else()
    target_compile_options(agsimuse PRIVATE -Wall -Wextra -Wpedantic)
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
            add_custom_command(TARGET imuse_sysex_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" \"$<TARGET_FILE:imuse_sysex_gui>\"
            )
            add_custom_command(TARGET imuse_player_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" \"$<TARGET_FILE:imuse_player_gui>\"
            )
            add_custom_command(TARGET imuse_packer_gui POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" \"$<TARGET_FILE:imuse_packer_gui>\"
            )
        endif()
    endif()
endif()
