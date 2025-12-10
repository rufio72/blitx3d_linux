cmake_minimum_required(VERSION 3.24)

add_executable(docgen docgen.cpp)

target_compile_features(docgen PRIVATE cxx_std_17)

install(TARGETS docgen DESTINATION bin)
