cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER_FORCED FALSE)

project(DrumSynthCell VERSION 0.1.0 LANGUAGES CXX)

get_filename_component(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE-framework" ABSOLUTE)

if(NOT EXISTS "${JUCE_DIR}")
    message(FATAL_ERROR "JUCE framework not found at: ${JUCE_DIR}")
endif()

message(STATUS "Using JUCE from: ${JUCE_DIR}")

list(APPEND CMAKE_MODULE_PATH "${JUCE_DIR}/cmake")

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_TOOLS  OFF CACHE BOOL "" FORCE)

if(APPLE AND CMAKE_VERSION VERSION_GREATER_EQUAL 4.3.0)
    set(CMAKE_C_COMPILE_OBJECT "/usr/bin/clang <DEFINES> <INCLUDES> <FLAGS> -fno-strict-aliasing -o <OBJECT> -c <SOURCE>")
endif()

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

add_subdirectory(Source)
