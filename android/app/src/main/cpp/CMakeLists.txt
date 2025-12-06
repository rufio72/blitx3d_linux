# Blitz3D-NG Android Native Build
# This CMakeLists.txt is used by Android Gradle to compile native code

cmake_minimum_required(VERSION 3.22.1)

project(blitz3dng)

# =====================================================
# SDL2 CONFIGURATION
# =====================================================
# Path to SDL2 source (adjust as needed)
set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../deps/sdl/tree)

# Add SDL2 as a subdirectory
add_subdirectory(${SDL2_DIR} SDL2)

# =====================================================
# BLITZ3D GAME LIBRARY
# =====================================================
# The "main" library contains your compiled Blitz3D game

add_library(main SHARED
    # Add your source files here
    # For now, this is a placeholder that will be replaced
    # by the compiled Blitz3D code
    main.cpp
)

# =====================================================
# INCLUDE DIRECTORIES
# =====================================================
target_include_directories(main PRIVATE
    ${SDL2_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# =====================================================
# LINK LIBRARIES
# =====================================================
target_link_libraries(main
    SDL2
    SDL2main
    android
    log
    GLESv2
    GLESv3
    EGL
    OpenSLES
)

# =====================================================
# COMPILER OPTIONS
# =====================================================
target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -O2
)

# Enable C++17
set_target_properties(main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
