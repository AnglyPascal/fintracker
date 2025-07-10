cmake_minimum_required(VERSION 4.0)
project(MarketMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -D_GLIBCXX_DEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET fin)


find_package(date REQUIRED)

# Use FetchContent to pull dependencies
include(FetchContent)

FetchContent_Declare(
  cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.12.0
) 
FetchContent_MakeAvailable(cpr)

# nlohmann/json - JSON parsing
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

# Add your source files
add_executable(${TARGET}
  src/main.cpp
  src/api.cpp
  src/format.cpp
  src/indicators.cpp
  src/signal.cpp
  src/portfolio.cpp
  src/positions.cpp
  src/notifier.cpp
  src/prediction.cpp
  src/webpage.cpp
  src/calendar.cpp
)

target_include_directories(${TARGET} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${TARGET}
  cpr::cpr
)

# Add your source files
add_executable(get_calendar
  src/get_calendar.cpp
)

target_link_libraries(get_calendar
  cpr::cpr
)

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
             TD_API_KEY_1 TD_API_KEY_2 TD_API_KEY_3
             TG_TOKEN TG_CHAT_ID TG_USER
             POSITIONS_FILE
             PATH HOME)
