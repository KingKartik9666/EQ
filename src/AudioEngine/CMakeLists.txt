add_executable(AudioEngine
    src/main.cpp
    src/endpoint_enumerator.cpp)

target_include_directories(AudioEngine PRIVATE include)
target_compile_features(AudioEngine PRIVATE cxx_std_20)
target_link_libraries(AudioEngine PRIVATE ole32 uuid avrt)

if(MSVC)
    target_compile_options(AudioEngine PRIVATE /W4 /permissive- /EHsc)
endif()
