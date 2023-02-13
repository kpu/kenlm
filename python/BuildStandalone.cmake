cmake_minimum_required(VERSION 3.1)

file(GLOB
  KENLM_PYTHON_STANDALONE_SRCS
  "util/*.cc"
  "lm/*.cc"
  "util/double-conversion/*.cc"
  "python/*.cc"
  )

list(FILTER KENLM_PYTHON_STANDALONE_SRCS EXCLUDE REGEX ".*main.cc")
list(FILTER KENLM_PYTHON_STANDALONE_SRCS EXCLUDE REGEX ".*test.cc")

add_library(
  kenlm
  SHARED
  ${KENLM_PYTHON_STANDALONE_SRCS}
  )

target_include_directories(kenlm PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_definitions(kenlm PRIVATE KENLM_MAX_ORDER=${KENLM_MAX_ORDER})

find_package(ZLIB)
find_package(BZip2)
find_package(LibLZMA)

if (ZLIB_FOUND)
  target_link_libraries(kenlm PRIVATE ${ZLIB_LIBRARIES})
  target_include_directories(kenlm PRIVATE ${ZLIB_INCLUDE_DIRS})
  target_compile_definitions(kenlm PRIVATE HAVE_ZLIB)
endif()
if(BZIP2_FOUND)
  target_link_libraries(kenlm PRIVATE ${BZIP2_LIBRARIES})
  target_include_directories(kenlm PRIVATE ${BZIP2_INCLUDE_DIR})
  target_compile_definitions(kenlm PRIVATE HAVE_BZLIB)
endif()
if(LIBLZMA_FOUND)
  target_link_libraries(kenlm PRIVATE ${LIBLZMA_LIBRARIES})
  target_include_directories(kenlm PRIVATE ${LIBLZMA_INCLUDE_DIRS})
  target_compile_definitions(kenlm PRIVATE HAVE_LZMA)
endif()
