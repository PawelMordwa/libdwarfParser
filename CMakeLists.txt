cmake_minimum_required(VERSION 3.10)
project(libdwarfParser VERSION 1.0 LANGUAGES CXX)

# Wymagaj C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opcje kompilacji
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Znajdź bibliotekę libdwarf
find_library(DWARF_LIBRARY dwarf REQUIRED)

# Pliki źródłowe
set(SOURCES
    main.cpp
    file_descriptor.cpp
    dwarf_utils.cpp
    type_cache.cpp
    type_info.cpp
    die_processor.cpp
)

# Pliki nagłówkowe
set(HEADERS
    file_descriptor.h
    dwarf_utils.h
    type_cache.h
    type_info.h
    die_processor.h
)

# Tworzenie executable
add_executable(dwarf_reader ${SOURCES} ${HEADERS})

# Linkowanie z libdwarf
target_link_libraries(dwarf_reader ${DWARF_LIBRARY})

# Instalacja
install(TARGETS dwarf_reader DESTINATION bin)
