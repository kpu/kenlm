find_package(Eigen3 REQUIRED)
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
    tune_instance.cc
    universal_vocab.cc)

add_library(kenlm_interpolate OBJECT ${KENLM_INTERPOLATE_SOURCE})

set(KENLM_INTERPOLATE_EXES
    interpolate
    perf_enum_gv
    streaming_example
    train_params
    tune)

set(KENLM_INTERPOLATE_DEPENDS
    $<TARGET_OBJECTS:kenlm>
    $<TARGET_OBJECTS:kenlm_util>
    $<TARGET_OBJECTS:kenlm_common>
    $<TARGET_OBJECTS:kenlm_interpolate>)

AddExes(EXES ${KENLM_INTERPOLATE_EXES}
        DEPENDS ${KENLM_INTERPOLATE_DEPENDS}
        LIBRARIES ${Boost_LIBRARIES} pthread)

if(BUILD_TESTING)
    set(KENLM_INTERPOLATE_TESTS
        backoff_reunification_test
        bounded_sequence_encoding_test
        merge_vocab_test
        normalize_test
        tune_derivatives_test)

    AddTests(TESTS ${KENLM_INTERPOLATE_TESTS}
             DEPENDS ${KENLM_INTERPOLATE_DEPENDS}
             LIBRARIES ${Boost_LIBRARIES} pthread)

    # tune_instance_test needs an extra command line parameter
    KenLMAddTest(TEST tune_instance_test
                 DEPENDS ${KENLM_INTERPOLATE_DEPENDS}
                 LIBRARIES ${Boost_LIBRARIES} pthread
                 TEST_ARGS
                 ${CMAKE_CURRENT_SOURCE_DIR}/tune_instance_data/toy0.1)
endif()
