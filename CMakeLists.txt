cmake_minimum_required(VERSION 3.18)

# ---- configuration ----
project(passpawn C CXX)
add_subdirectory(thirdparty EXCLUDE_FROM_ALL)

set(CMAKE_SHARED_LIBRARY_PREFIX "")


# ---- library: codec ----
add_library(passpawn-codec SHARED)
target_include_directories(passpawn-codec PRIVATE .)
target_sources(passpawn-codec
  PRIVATE
    codec/_init.cc
    codec/stb_image.cc
)
target_link_libraries(passpawn-codec
  PRIVATE
    stb
)
