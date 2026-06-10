cmake_minimum_required(VERSION 3.24)

add_executable(docgen docgen.cpp)

target_compile_features(docgen PRIVATE cxx_std_17)

target_output_to_bin(docgen)

install(TARGETS docgen DESTINATION bin)

# Generate the help files (index.html etc.) needed by the IDE help browser.
add_custom_command(TARGET docgen POST_BUILD
  COMMAND $<TARGET_FILE:docgen> -s ${CMAKE_SOURCE_DIR} -o ${RELEASE_PATH}/help
  COMMENT "Generating documentation in ${RELEASE_PATH}/help"
)
