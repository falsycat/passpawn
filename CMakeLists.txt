cmake_minimum_required(VERSION 3.18)

# ---- configuration ----
project(passpawn C CXX)
add_subdirectory(thirdparty EXCLUDE_FROM_ALL)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PASSPAWN_OPTIONS_WARNING
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Werror -pedantic-errors -Wextra -Wconversion -Wsign-conversion>
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -Wno-overloaded-virtual>
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4 /WX /Zc:__cplusplus /external:anglebrackets /external:W0>
)


# ---- library: codec ----
add_library(passpawn-codec SHARED)
target_include_directories(passpawn-codec PRIVATE .)
target_compile_options(passpawn-codec PRIVATE ${PASSPAWN_OPTIONS_WARNING})
target_sources(passpawn-codec
  PRIVATE
    nf7.hh
    common/queue.hh
    common/value.hh

    codec/_init.cc
    codec/stb_image.cc
    codec/zlib.cc
)
target_link_libraries(passpawn-codec
  PRIVATE
    stb
    zlibstatic
)

# ---- library: io ----
add_library(passpawn-io SHARED)
target_include_directories(passpawn-io PRIVATE .)
target_compile_options(passpawn-io PRIVATE ${PASSPAWN_OPTIONS_WARNING})
target_sources(passpawn-io
  PRIVATE
    nf7.hh
    common/queue.hh
    common/value.hh

    io/_init.cc
    io/nfile.cc
)
