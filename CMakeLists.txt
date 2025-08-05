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

find_library(ZMQ_LIB zmq REQUIRED)
find_path(ZMQ_INCLUDE_DIR zmq.hpp REQUIRED)


# Add your source files
add_executable(${TARGET}
  src/main.cpp
  src/times.cpp
  src/config.cpp
  src/api.cpp
  src/format.cpp
  src/indicators.cpp
  src/signals.cpp
  src/signal_funcs.cpp
  src/portfolio.cpp
  src/positions.cpp
  src/position_sizing.cpp
  src/notifier.cpp
  src/prediction.cpp
  src/webpage.cpp
  src/calendar.cpp
  src/backtest.cpp
  src/replay.cpp
)

target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${TARGET} SYSTEM PRIVATE ${ZMQ_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${SPDLOG_INCLUDE_DIR})
target_include_directories(${TARGET} SYSTEM PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})

target_link_libraries(${TARGET}
  cpr::cpr
  argparse
  ${ZMQ_LIB}
)

set(EMBED_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_str.py)
set(INPUT_HTML 
    ${CMAKE_CURRENT_SOURCE_DIR}/data/index.html 
    ${CMAKE_CURRENT_SOURCE_DIR}/data/ticker.html)
set(OUTPUT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/include/_html_template.h)

add_custom_command(
  OUTPUT ${OUTPUT_HEADER} 
  COMMAND python ${EMBED_SCRIPT}
  DEPENDS ${INPUT_HTML} ${EMBED_SCRIPT} 
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(html_header DEPENDS ${OUTPUT_HEADER})
add_dependencies(${TARGET} html_header)

function(add_env_vars TARGET)
    # Remaining args are the list of environment variable names
    foreach(VAR_NAME IN LISTS ARGN)
        if(DEFINED ENV{${VAR_NAME}})
            set(VAR_VALUE "$ENV{${VAR_NAME}}")
            target_compile_definitions(${TARGET} PRIVATE ${VAR_NAME}="${VAR_VALUE}")
        else()
            message(WARNING "Environment variable ${VAR_NAME} is not defined.")
        endif()
    endforeach()
endfunction()

add_env_vars(${TARGET} 
             TD_API_KEY_1 TD_API_KEY_2 TD_API_KEY_3 TD_API_KEY_4 TD_API_KEY_5
             TG_TOKEN TG_CHAT_ID TG_USER
             PATH HOME)


set(CALENDAR get_calendar)

add_executable(${CALENDAR}
  src/get_calendar.cpp
)

target_link_libraries(${CALENDAR} cpr::cpr)
target_include_directories(${CALENDAR} SYSTEM PRIVATE ${SPDLOG_INCLUDE_DIR})
target_include_directories(${CALENDAR} SYSTEM PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})

