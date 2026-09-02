add_executable(AudioService
    src/main.cpp
    src/per_user_audio_host.cpp)

add_executable(AdaptiveAudio::Service ALIAS AudioService)

target_include_directories(AudioService
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(AudioService PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(AudioService PRIVATE /W4 /permissive- /EHsc)
endif()
