file(REMOVE_RECURSE
  "libfluidsynth.a"
  "libfluidsynth.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C CXX)
  include(CMakeFiles/libfluidsynth.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
