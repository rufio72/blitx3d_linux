IF(TARGET ogg)
  set(CMAKE_UNITY_BUILD false)

  # use ogg in project tree (guard keeps the replace idempotent across
  # reconfigures, otherwise each configure prepends another "# ")
  file(READ tree/CMakeLists.txt FILE_CONTENTS)
  if(NOT FILE_CONTENTS MATCHES "# find_package\\(Ogg")
    string(REPLACE "find_package(Ogg" "# find_package(Ogg" FILE_CONTENTS "${FILE_CONTENTS}")
    file_write_changes(tree/CMakeLists.txt "${FILE_CONTENTS}")
  endif()

  add_subdirectory(tree)
  target_output_to_toolchain(vorbis)
  target_output_to_toolchain(vorbisfile)
ENDIF()
