# Install script for directory: /Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Library/Developer/CommandLineTools/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_program" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/fluidsynth")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/fluidsynth" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/fluidsynth")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/fluidsynth")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/libfluidsynth.a")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libfluidsynth.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libfluidsynth.a")
    execute_process(COMMAND "/Library/Developer/CommandLineTools/usr/bin/ranlib" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libfluidsynth.a")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/fluidsynth" TYPE FILE FILES
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/audio.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/event.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/gen.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/ladspa.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/log.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/midi.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/misc.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/mod.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/seq.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/seqbind.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/settings.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/sfont.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/shell.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/synth.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/types.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/include/fluidsynth/voice.h"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/include/fluidsynth/version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/include/fluidsynth.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-static-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-static-targets.cmake"
         "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-static-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-static-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth/FluidSynth-static-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth" TYPE FILE FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-static-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth" TYPE FILE FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/CMakeFiles/Export/6a6ea4714c6c3916014c1d187313a071/FluidSynth-static-targets-relwithdebinfo.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
