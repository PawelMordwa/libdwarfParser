cmake_minimum_required(VERSION 3.10)
project(libdwarfParser VERSION 1.0 LANGUAGES C CXX)

# Wymagaj C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opcje kompilacji
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Pobierz libdwarf z GitHub
include(FetchContent)

FetchContent_Declare(
    libdwarf
    GIT_REPOSITORY https://github.com/davea42/libdwarf-code.git
    GIT_TAG        v2.2.0
    SOURCE_SUBDIR  cmake
)

# Opcje libdwarf - wyłącz niepotrzebne rzeczy
set(BUILD_DWARFDUMP OFF CACHE BOOL "Don't build dwarfdump" FORCE)
set(BUILD_DWARFGEN OFF CACHE BOOL "Don't build dwarfgen" FORCE)
set(BUILD_DWARFEXAMPLE OFF CACHE BOOL "Don't build examples" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "Build static library" FORCE)
set(DO_TESTING OFF CACHE BOOL "Don't build tests" FORCE)

message(STATUS "Pobieranie libdwarf...")
FetchContent_MakeAvailable(libdwarf)

# Znajdź katalog z nagłówkami libdwarf
set(LIBDWARF_INCLUDE_DIR "${libdwarf_SOURCE_DIR}/src/lib/libdwarf")
set(LIBDWARF_BUILD_INCLUDE_DIR "${libdwarf_BINARY_DIR}/src/lib/libdwarf")

# Pliki źródłowe
set(SOURCES
    main.cpp
    file_descriptor.cpp
    dwarf_utils.cpp
    type_cache.cpp
    type_info.cpp
    die_processor.cpp
    variable_info.cpp
)

# Pliki nagłówkowe
set(HEADERS
    file_descriptor.h
    dwarf_utils.h
    type_cache.h
    type_info.h
    die_processor.h
    variable_info.h
)

# Tworzenie executable
add_executable(dwarf_reader ${SOURCES} ${HEADERS})

# Dodaj katalogi z nagłówkami libdwarf
target_include_directories(dwarf_reader PRIVATE 
    ${LIBDWARF_INCLUDE_DIR}
    ${LIBDWARF_BUILD_INCLUDE_DIR}
)

# Linkowanie z libdwarf
target_link_libraries(dwarf_reader PRIVATE dwarf)

# Instalacja
install(TARGETS dwarf_reader DESTINATION bin)
