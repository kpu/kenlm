cmake_minimum_required(VERSION 2.8.8)
#
# The KenLM cmake files make use of add_library(... OBJECTS ...)
# 
# This syntax allows grouping of source files when compiling
# (effectively creating "fake" libraries based on source subdirs).
# 
# This syntax was only added in cmake version 2.8.8
#
# see http://www.cmake.org/Wiki/CMake/Tutorials/Object_Library


# This CMake file was created by Lane Schwartz <dowobeha@gmail.com>


set(KENLM_MAX_ORDER 6 CACHE STRING "Maximum supported ngram order")

add_definitions(-DKENLM_MAX_ORDER=${KENLM_MAX_ORDER})


# Explicitly list the source files for this subdirectory
#
# If you add any source files to this subdirectory
#    that should be included in the kenlm library,
#        (this excludes any unit test files)
#    you should add them to the following list:
set(KENLM_SOURCE 
	bhiksha.cc
	binary_format.cc
	config.cc
	lm_exception.cc
	model.cc
	quantize.cc
	read_arpa.cc
	search_hashed.cc
	search_trie.cc
	sizes.cc
	trie.cc
	trie_sort.cc
	value_build.cc
	virtual_interface.cc
	vocab.cc
)


# Group these objects together for later use. 
#
# Given add_library(foo OBJECT ${my_foo_sources}),
# refer to these objects as $<TARGET_OBJECTS:foo>
#
add_library(kenlm OBJECT ${KENLM_SOURCE})

# This directory has children that need to be processed
add_subdirectory(builder)
add_subdirectory(common)
add_subdirectory(filter)



# Explicitly list the executable files to be compiled
set(EXE_LIST
  query
  fragment
  build_binary
)

AddExes(EXES ${EXE_LIST}
        DEPENDS $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_util>
        LIBRARIES ${Boost_LIBRARIES} pthread)

# Conditionally build the interpolation code
if(BUILD_INTERPOLATE)
  add_subdirectory(interpolate)
endif()

if(BUILD_TESTING)

  set(KENLM_BOOST_TESTS_LIST left_test partial_test)
  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
           DEPENDS $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_util>
           LIBRARIES ${Boost_LIBRARIES} pthread
           TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test.arpa)

  # model_test requires an extra command line parameter
  KenLMAddTest(TEST model_test
               DEPENDS $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_util>
               LIBRARIES ${Boost_LIBRARIES} pthread
               TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test.arpa
                         ${CMAKE_CURRENT_SOURCE_DIR}/test_nounk.arpa)
endif()
