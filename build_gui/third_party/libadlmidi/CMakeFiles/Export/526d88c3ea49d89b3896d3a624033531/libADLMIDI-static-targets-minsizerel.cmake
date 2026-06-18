#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libADLMIDI::ADLMIDI_static" for configuration "MinSizeRel"
set_property(TARGET libADLMIDI::ADLMIDI_static APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(libADLMIDI::ADLMIDI_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C;CXX"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/ADLMIDI.lib"
  )

list(APPEND _cmake_import_check_targets libADLMIDI::ADLMIDI_static )
list(APPEND _cmake_import_check_files_for_libADLMIDI::ADLMIDI_static "${_IMPORT_PREFIX}/lib/ADLMIDI.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
