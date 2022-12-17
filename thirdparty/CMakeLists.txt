include(FetchContent)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 OLD)
endif()

set(BUILD_SHARED_LIBS OFF)


# ---- stb ----
# repository: https://github.com/nothings/stb
# license   : Unlicense
FetchContent_Declare(
  stb
  URL "https://github.com/nothings/stb/archive/8b5f1f37b5b75829fc72d38e7b5d4bcbf8a26d55.zip"
)
FetchContent_Populate(stb)

add_library(stb)
target_include_directories(stb SYSTEM PUBLIC ${stb_SOURCE_DIR})
target_sources(stb
  PUBLIC
    ${stb_SOURCE_DIR}/stb_image.h
  PRIVATE
    stb.c
)


