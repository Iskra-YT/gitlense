# Znalezienie lub pobranie libgit2.
#
# Kolejnosc:
#   1. lokalny prefiks (CMAKE_PREFIX_PATH / GITLENSE_LIBGIT2_ROOT)
#   2. systemowy pakiet (libgit2-dev)
#   3. FetchContent - budowa ze zrodel, bez uprawnien roota

if(DEFINED GITLENSE_LIBGIT2_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${GITLENSE_LIBGIT2_ROOT}")
endif()

set(GITLENSE_LIBGIT2_VERSION "1.9.7" CACHE STRING "Wersja libgit2 pobierana ze zrodel")

find_package(libgit2 1.7 QUIET CONFIG)

if(libgit2_FOUND)
    message(STATUS "gitlense: libgit2 ${libgit2_VERSION} (system)")
elseif(GITLENSE_FETCH_LIBGIT2)
    message(STATUS "gitlense: brak libgit2 w systemie, pobieram ${GITLENSE_LIBGIT2_VERSION} ze zrodel")
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_TESTS OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_BUILD_CLI OFF CACHE BOOL "" FORCE)
    # Wywlamy opcjonalne zaleznosci systemowe, zeby build dzialal bez roota.
    set(LIBGIT2_USE_SSH OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_HTTPS OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_USE_BUNDLED_ZLIB ON CACHE BOOL "" FORCE)
    set(LIBGIT2_USE_BUNDLED_PCRE OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_USE_BUNDLED_LIBMDC OFF CACHE BOOL "" FORCE)
    set(LIBGIT2_USE_THREADS ON CACHE BOOL "" FORCE)
    set(REGEX_BACKEND "" CACHE STRING "" FORCE)
    set(USE_SSH OFF CACHE BOOL "" FORCE)
    set(USE_HTTPS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(libgit2
        GIT_REPOSITORY https://github.com/libgit2/libgit2.git
        GIT_TAG v${GITLENSE_LIBGIT2_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(libgit2)
else()
    message(FATAL_ERROR
        "Brak libgit2 >= 1.7. Zainstaluj libgit2-dev albo wlacz "
        "-DGITLENSE_FETCH_LIBGIT2=ON (domyslnie wlaczone).")
endif()

# Wybierz konkretny cel: paczka zrodlowa libgit2 1.9 eksportuje tylko
# "libgit2package" (statyczna liba) i nie przekazuje sciezki include.
if(TARGET libgit2::libgit2)
    set(GITLENSE_LIBGIT2_TARGET libgit2::libgit2)
elseif(TARGET libgit2::libgit2package)
    set(GITLENSE_LIBGIT2_TARGET libgit2::libgit2package)
elseif(TARGET libgit2)
    set(GITLENSE_LIBGIT2_TARGET libgit2)
else()
    add_library(libgit2::libgit2 INTERFACE IMPORTED GLOBAL)
    set(GITLENSE_LIBGIT2_TARGET libgit2::libgit2)
endif()
add_library(Gitlense::libgit2 ALIAS ${GITLENSE_LIBGIT2_TARGET})

# Gdy paczka nie eksportuje include dirs, dodaj je recznie.
if(libgit2_FOUND AND libgit2_DIR)
    get_filename_component(_gitlense_libgit2_prefix "${libgit2_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_gitlense_libgit2_prefix}/include/git2.h")
        set_property(TARGET ${GITLENSE_LIBGIT2_TARGET}
            APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_gitlense_libgit2_prefix}/include")
    elseif(EXISTS "${_gitlense_libgit2_prefix}/include/git2/repository.h")
        set_property(TARGET ${GITLENSE_LIBGIT2_TARGET}
            APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${_gitlense_libgit2_prefix}/include/git2")
    endif()
    unset(_gitlense_libgit2_prefix)
endif()
