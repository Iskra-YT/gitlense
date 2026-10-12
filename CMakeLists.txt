cmake_minimum_required(VERSION 3.20)

project(gitlense
    VERSION 0.1.0
    DESCRIPTION "Terminalowa przegladarka historii i statystyk repozytorium Git"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Typ builda" FORCE)
endif()

option(GITLENSE_BUILD_TESTS "Buduj testy" ON)
option(GITLENSE_WERROR "Traktuj ostrzezenia jako bledy" OFF)
option(GITLENSE_FETCH_LIBGIT2 "Pobierz libgit2 ze zrodel, gdy nie ma go w systemie" ON)

include(cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# gitlense_core - cala logika, bez parsowania argv
# ---------------------------------------------------------------------------
add_library(gitlense_core STATIC
    src/core/libgit2.cpp
    src/core/types.cpp
    src/core/repository.cpp
    src/core/walker.cpp
    src/core/language.cpp
    src/format/color.cpp
    src/format/table.cpp
    src/format/chart.cpp
    src/format/format.cpp
    src/analysis/history.cpp
    src/analysis/line_counts.cpp
    src/analysis/contributors.cpp
    src/analysis/languages.cpp
    src/analysis/hotspots.cpp
    src/analysis/activity.cpp
    src/cli/args.cpp
    src/cli/progress.cpp
)

target_include_directories(gitlense_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gitlense_core PUBLIC Gitlense::libgit2)
target_compile_definitions(gitlense_core PUBLIC
    GITLENSE_VERSION="${PROJECT_VERSION}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gitlense_core PRIVATE -Wall -Wextra -Wpedantic)
    if(GITLENSE_WERROR)
        target_compile_options(gitlense_core PRIVATE -Werror)
    endif()
elseif(MSVC)
    target_compile_options(gitlense_core PRIVATE /W4)
    if(GITLENSE_WERROR)
        target_compile_options(gitlense_core PRIVATE /WX)
    endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(gitlense_core PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# gitlense - biblioteka + wszystkie podkomendy + main
# ---------------------------------------------------------------------------
add_library(gitlense_commands STATIC
    src/commands/registry.cpp
    src/commands/cmd_log.cpp
    src/commands/cmd_show.cpp
    src/commands/cmd_file.cpp
    src/commands/cmd_branches.cpp
    src/commands/cmd_diff.cpp
    src/commands/cmd_contributors.cpp
    src/commands/cmd_languages.cpp
    src/commands/cmd_hotspots.cpp
    src/commands/cmd_activity.cpp
    src/commands/cmd_shortlog.cpp
    src/commands/cmd_search.cpp
    src/commands/cmd_graph.cpp
)

target_link_libraries(gitlense_commands PUBLIC gitlense_core)

add_executable(gitlense src/main.cpp)
target_link_libraries(gitlense PRIVATE gitlense_commands)

install(TARGETS gitlense RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Testy
# ---------------------------------------------------------------------------
if(GITLENSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
