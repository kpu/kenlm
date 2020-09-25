# This CMake file was created by Lane Schwartz <dowobeha@gmail.com>

# Explicitly list the source files for this subdirectory
#
# If you add any source files to this subdirectory
#    that should be included in the kenlm library,
#        (this excludes any unit test files)
#    you should add them to the following list:
#
# In order to set correct paths to these files
#    in case this variable is referenced by CMake files in the parent directory,
#    we prefix all files with ${CMAKE_CURRENT_SOURCE_DIR}.
#
set(KENLM_BUILDER_SOURCE
		${CMAKE_CURRENT_SOURCE_DIR}/adjust_counts.cc
		${CMAKE_CURRENT_SOURCE_DIR}/corpus_count.cc
		${CMAKE_CURRENT_SOURCE_DIR}/initial_probabilities.cc
		${CMAKE_CURRENT_SOURCE_DIR}/interpolate.cc
		${CMAKE_CURRENT_SOURCE_DIR}/output.cc
		${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cc
	)


# Group these objects together for later use.
#
# Given add_library(foo OBJECT ${my_foo_sources}),
# refer to these objects as $<TARGET_OBJECTS:foo>
#
add_library(kenlm_builder ${KENLM_BUILDER_SOURCE})

target_link_libraries(kenlm_builder PUBLIC kenlm kenlm_util ${Boost_LIBRARIES} Threads::Threads)

AddExes(EXES lmplz
        LIBRARIES kenlm_builder kenlm kenlm_util ${Boost_LIBRARIES} Threads::Threads)
AddExes(EXES count_ngrams
        LIBRARIES kenlm_builder kenlm kenlm_util ${Boost_LIBRARIES} Threads::Threads)

if(BUILD_TESTING)

  # Explicitly list the Boost test files to be compiled
  set(KENLM_BOOST_TESTS_LIST
    adjust_counts_test
    corpus_count_test
  )

  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
           LIBRARIES kenlm_builder kenlm kenlm_util ${Boost_LIBRARIES} Threads::Threads)
endif()
