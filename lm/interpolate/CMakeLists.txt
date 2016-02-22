find_package(Eigen3)

if(EIGEN3_FOUND AND 3.1.0 VERSION_LESS ${EIGEN3_VERSION})
  include_directories(${EIGEN3_INCLUDE_DIR})
  
  set(KENLM_INTERPOLATE_SOURCE
      backoff_reunification.cc
      bounded_sequence_encoding.cc
      enumerate_global_vocab.cc
      merge_probabilities.cc
      merge_vocab.cc
      normalize.cc
      pipeline.cc
      split_worker.cc
      tune_derivatives.cc
      tune_instances.cc
      tune_weights.cc
      universal_vocab.cc)
  
  add_library(kenlm_interpolate ${KENLM_INTERPOLATE_SOURCE})
  
  set(KENLM_INTERPOLATE_EXES
      interpolate
      streaming_example)
  
    set(KENLM_INTERPOLATE_LIBS
      kenlm_interpolate kenlm kenlm_util ${Boost_LIBRARIES} pthread)
  
  AddExes(EXES ${KENLM_INTERPOLATE_EXES}
          LIBRARIES ${KENLM_INTERPOLATE_LIBS})
  
  if(BUILD_TESTING)
      set(KENLM_INTERPOLATE_TESTS
          backoff_reunification_test
          bounded_sequence_encoding_test
          merge_vocab_test
          normalize_test
          tune_derivatives_test)
  
      AddTests(TESTS ${KENLM_INTERPOLATE_TESTS}
               LIBRARIES ${KENLM_INTERPOLATE_LIBS} pthread)
  
      # tune_instances_test needs an extra command line parameter
      KenLMAddTest(TEST tune_instances_test
                   DEPENDS ${KENLM_INTERPOLATE_DEPENDS}
                   LIBRARIES ${KENLM_INTERPOLATE_LIBS}
                   TEST_ARGS
                   ${CMAKE_CURRENT_SOURCE_DIR}/tune_instances_data/toy0.1)
  endif()
else()
  if (EIGEN3_FOUND)
    message(WARNING "Not building interpolation. You have an old version of Eigen3, ${EIGEN3_VERSION}, which has a race condition: http://eigen.tuxfamily.org/bz/show_bug.cgi?id=466.  Please install Eigen 3.1.0 or above.")
  else()
    message(WARNING "Not building interpolation. Eigen3 was not found.")
  endif()
  message(STATUS "To install Eigen3 in your home directory, copy paste this:\n"
      "export EIGEN3_ROOT=$HOME/eigen-eigen-07105f7124f9\n"
      "(cd $HOME; wget -O - https://bitbucket.org/eigen/eigen/get/3.2.8.tar.bz2 |tar xj)\n"
      "rm CMakeCache.txt\n")
endif()
