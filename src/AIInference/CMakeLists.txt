add_library(AIInference STATIC
    src/inference_runtime.cpp)

add_library(AdaptiveAudio::AIInference ALIAS AIInference)

target_include_directories(AIInference
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(AIInference PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(AIInference PRIVATE /W4 /permissive- /EHsc)
endif()
