# Install script for directory: /Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master

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

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/fluidsynth.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fluidsynth_development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fluidsynth" TYPE FILE FILES
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/FluidSynthConfig.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/FluidSynthConfigVersion.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindFLAC.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindGCEM.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindGLib2.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindInstPatch.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindJack.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindMidiShare.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindOgg.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindOpenSLES.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindOpus.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindPipeWire.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindPortAudio.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindReadline.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindSndFileLegacy.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindSystemd.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/FindVorbis.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/Findlibffi.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/Findmp3lame.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/Findmpg123.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/Findoboe.cmake"
    "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-master/cmake_admin/PkgConfigHelpers.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/src/cmake_install.cmake")
  include("/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/test/cmake_install.cmake")
  include("/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/doc/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/komasami/Dev/scumm-tools/imuse-v6/third_party/fluidsynth-build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
