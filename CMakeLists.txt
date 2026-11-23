cmake_minimum_required(VERSION 3.25)
project(Unfairboy067 VERSION 0.1.0 LANGUAGES C CXX) # C is required by some JUCE modules

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TWINMU_BUILD_TESTS  "Build the DSP unit tests" ON)
option(TWINMU_BUILD_PLUGIN "Build the JUCE plugin (VST3/AU/Standalone)" OFF)

# ------------------------------------------------------------------ DSP core (no JUCE)
add_library(twinmu_dsp STATIC
    Source/DSP/Engine.cpp
    Source/DSP/Limiter.cpp
    Source/DSP/Oversampling.cpp)
target_include_directories(twinmu_dsp PUBLIC Source)
if(MSVC)
    target_compile_options(twinmu_dsp PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(twinmu_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------ tests
if(TWINMU_BUILD_TESTS)
    include(FetchContent)
    # Only the single header is used: SOURCE_SUBDIR points to a folder without CMakeLists,
    # so doctest's own (old) CMake project is not configured.
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
        SOURCE_SUBDIR do-not-configure)
    FetchContent_MakeAvailable(doctest)

    enable_testing()
    add_executable(twinmu_tests
        Tests/TestMain.cpp
        Tests/DspUnitTests.cpp
        Tests/EngineTests.cpp)
    target_include_directories(twinmu_tests PRIVATE ${doctest_SOURCE_DIR}/doctest Tests)
    target_link_libraries(twinmu_tests PRIVATE twinmu_dsp)
    if(MSVC)
        target_compile_options(twinmu_tests PRIVATE /utf-8 /bigobj)
    endif()
    add_test(NAME twinmu_tests COMMAND twinmu_tests)
endif()

# ------------------------------------------------------------------ plugin (Phase 3/4)
if(TWINMU_BUILD_PLUGIN)
    add_subdirectory(Plugin)
endif()
