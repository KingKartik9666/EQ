add_executable(AdaptiveAudio
    src/main.cpp
    src/controller.cpp)

add_executable(AdaptiveAudio::App ALIAS AdaptiveAudio)

target_include_directories(AdaptiveAudio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(AdaptiveAudio PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(AdaptiveAudio PRIVATE /W4 /permissive- /EHsc)
endif()
