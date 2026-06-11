if(NOT BB_DESKTOP)
  return()
endif()

# importers are opt-in to keep build time and binary size down; these are
# the interchange formats Blender & co. actually export
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

# use the in-tree zlib: pre-seeding these short-circuits assimp's
# FIND_PACKAGE(ZLIB), so ${ZLIB_LIBRARIES} resolves to our target and the
# build never depends on a system zlib (ASSIMP_BUILD_ZLIB would instead
# define a second "zlibstatic" target and clash with deps/zlib)
# plain (directory-scoped) variables: a CACHE FORCE here would leak into
# every other find_package(ZLIB) in the tree (e.g. wxWidgets' export sets)
set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/deps/zlib/tree" "${CMAKE_BINARY_DIR}/deps/zlib/tree")

add_subdirectory(tree)

target_output_to_toolchain(assimp)
set_target_properties(assimp PROPERTIES UNITY_BUILD false)
