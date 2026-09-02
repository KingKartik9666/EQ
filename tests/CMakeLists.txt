add_executable(dsp_core_tests
    dsp_core_tests.cpp)

target_link_libraries(dsp_core_tests PRIVATE AdaptiveAudio::Dsp)
target_compile_features(dsp_core_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(dsp_core_tests PRIVATE /W4 /permissive- /EHsc)
endif()

add_test(NAME dsp_core_tests COMMAND dsp_core_tests)
