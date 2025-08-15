cmake_minimum_required(VERSION 3.11)

set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

project(MarketMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -D_GLIBCXX_DEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET fin)

# Use FetchContent to pull dependencies
include(FetchContent)

FetchContent_Declare(
  cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.12.0
) 
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

FetchContent_Declare(
  json 
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

get_target_property(
  NLOHMANN_JSON_INCLUDE_DIR 
  nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)

# Fetch spdlog (compiled version)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0  # Replace with the latest stable tag if needed
)

# Set this BEFORE FetchContent_MakeAvailable to build the compiled version
set(SPDLOG_BUILD_STATIC ON)
set(SPDLOG_HEADER_ONLY OFF)

FetchContent_MakeAvailable(spdlog)

get_target_property(
  SPDLOG_INCLUDE_DIR 
  spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)


FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG main
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

get_target_property(
  GLAZE_INCLUDE_DIR 
  glaze::glaze INTERFACE_INCLUDE_DIRECTORIES)


FetchContent_Declare(
    cpp-subprocess
    GIT_REPOSITORY https://github.com/arun11299/cpp-subprocess.git
)
FetchContent_MakeAvailable(cpp-subprocess)

get_target_property(
  SUBPROCESS_INCLUDE_DIR 
  cpp-subprocess::subprocess INTERFACE_INCLUDE_DIRECTORIES)


file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SOURCES 
 "${CMAKE_CURRENT_SOURCE_DIR}/src/get_calendar.cpp"
)
add_executable(${TARGET} ${SOURCES})

target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${TARGET} SYSTEM PRIVATE ${ZMQ_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${SPDLOG_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${GLAZE_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${SUBPROCESS_INCLUDE_DIR})

target_link_libraries(${TARGET} PRIVATE
  cpr::cpr
  argparse
)

set(EMBED_SCRIPT ${PROJECT_SOURCE_DIR}/scripts/embed_str.py)
set(INPUT_HTML 
    ${PROJECT_SOURCE_DIR}/data/index.html 
    ${PROJECT_SOURCE_DIR}/data/ticker.html)
set(OUTPUT_HEADER ${PROJECT_SOURCE_DIR}/include/util/gen_html_template.h)

add_custom_command(
  OUTPUT ${OUTPUT_HEADER} 
  COMMAND python ${EMBED_SCRIPT}
  DEPENDS ${INPUT_HTML} ${EMBED_SCRIPT}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Generating ${OUTPUT_HEADER}"
)

target_sources(${TARGET} PRIVATE ${OUTPUT_HEADER})

set(CALENDAR get_calendar)

add_executable(${CALENDAR}
  src/get_calendar.cpp
)

target_link_libraries(${CALENDAR} cpr::cpr)
target_include_directories(${CALENDAR} SYSTEM PRIVATE ${SPDLOG_INCLUDE_DIR})
target_include_directories(${CALENDAR} SYSTEM PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})

