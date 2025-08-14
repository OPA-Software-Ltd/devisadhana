cmake_minimum_required(VERSION 3.16)
project(sadhana LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SADHANA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
find_package(SQLite3 REQUIRED)

add_executable(sadhana
  src/main.cpp
  src/audio_input.cpp
  src/vad.cpp
  src/sqlite_logger.cpp
  src/config.cpp
  src/phrase_spec.cpp        # NEW
  src/phrase_match.cpp       # NEW
)

target_include_directories(sadhana PRIVATE src)

target_link_libraries(sadhana PRIVATE PkgConfig::PORTAUDIO SQLite::SQLite3)

if (MSVC)
  target_compile_options(sadhana PRIVATE /W4)
else()
  target_compile_options(sadhana PRIVATE -Wall -Wextra -Wpedantic)
  if (SADHANA_WARNINGS_AS_ERRORS)
    target_compile_options(sadhana PRIVATE -Werror)
  endif()
endif()

set_target_properties(sadhana PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

