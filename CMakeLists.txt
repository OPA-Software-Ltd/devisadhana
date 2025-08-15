cmake_minimum_required(VERSION 3.31)
project(untitled)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# Add Vosk
find_library(VOSK_LIBRARY vosk REQUIRED)

include_directories(
    ${PORTAUDIO_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/asr
    /usr/include/vosk  # Add this line - adjust path as needed
)

add_executable(untitled
    main.cpp
    src/audio/audio_capture.cpp
    src/audio/vad.cpp
    src/asr/vosk_asr.cpp
)

target_link_libraries(untitled
    ${PORTAUDIO_LIBRARIES}
    ${VOSK_LIBRARY}
    -lpthread
)

# Copy models directory
add_custom_command(TARGET untitled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_BINARY_DIR}/models
)