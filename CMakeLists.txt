
cmake_minimum_required(VERSION 3.31)
project(untitled)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary directory: ${CMAKE_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

find_library(VOSK_LIBRARY vosk REQUIRED)

find_package(nlohmann_json 3.11.2 REQUIRED)

include_directories(
        ${PORTAUDIO_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/phrase
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/asr
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/definition
        /usr/include/vosk
)

add_executable(untitled
        main.cpp
        src/audio/audio_capture.cpp
        src/audio/vad.cpp
        src/asr/vosk_asr.cpp
        src/definition/definition.cpp
        src/phrase/phrase_manager.cpp
)

target_link_libraries(untitled
        ${PORTAUDIO_LIBRARIES}
        ${VOSK_LIBRARY}
        -lpthread
        nlohmann_json::nlohmann_json
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(untitled stdc++fs)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/rituals/definitions/ganapati/maha_ganapati_caturvrtti_tarpanam.json")
    message(STATUS "Ritual definition file found in source directory")
else()
    message(WARNING "Ritual definition file NOT found in source directory")
endif()

add_custom_command(TARGET untitled PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/rituals
        ${CMAKE_BINARY_DIR}/rituals
)

add_custom_command(TARGET untitled POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models
        ${CMAKE_BINARY_DIR}/models
)