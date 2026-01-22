# libdwarfParser

Parser informacji DWARF z plików ELF, specjalnie zoptymalizowany dla procesorów Texas Instruments C2000.

## Struktura projektu

```
.
├── main.cpp              - Główny punkt wejścia programu
├── file_descriptor.h/cpp - Klasa RAII do zarządzania deskryptorami plików
├── dwarf_utils.h/cpp     - Funkcje pomocnicze (obsługa błędów, konwersje)
├── type_cache.h/cpp      - Cache dla sygnatur typów DWARF 4
├── type_info.h/cpp       - Funkcje do pobierania informacji o typach
├── die_processor.h/cpp   - Przetwarzanie DIE (Debug Information Entries)
├── CMakeLists.txt        - System budowania CMake (cross-platform)
└── README.md             - Ten plik
```

## Wymagania

- Kompilator C++ z obsługą C++11 lub nowszego (g++, clang++, MSVC)
- CMake 3.10 lub nowszy
- libdwarf (wersja 0.11.1 lub nowsza)

### Linux
```bash
# Debian/Ubuntu
sudo apt install cmake libdwarf-dev

# Fedora
sudo dnf install cmake libdwarf-devel
```

### Windows
- Zainstaluj [CMake](https://cmake.org/download/)
- Zainstaluj [Visual Studio](https://visualstudio.microsoft.com/) z komponentami C++
- Zbuduj lub pobierz libdwarf dla Windows

## Kompilacja

### Metoda 1: Użycie CMake Presets (Zalecane)

CMake Presets automatycznie tworzą katalog build i konfigurują projekt.

**Domyślna kompilacja:**
```bash
cmake --preset default
cmake --build --preset default
```

### Metoda 2: Tradycyjna metoda CMake

**Linux/macOS:**
```bash
mkdir build
cd build
cmake ..
make
```

**Windows (Visual Studio):**
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Instalacja

Linux/macOS:
```bash
cd build
sudo make install
```

Windows (uruchom jako Administrator):
```cmd
cd build
cmake --install .
```

## Użycie

```bash
./dwarf_reader <plik_elf>
```

Przykład:
```bash
./dwarf_reader ../lab_sci_launchpad.elf
```

## Funkcjonalności

- Parsowanie informacji DWARF z plików ELF
- Obsługa sekcji `.debug_types` (DWARF 4)
- Wyświetlanie zmiennych globalnych z ich adresami
- Informacje o typach (nazwa, rozmiar)
- Analiza struktur i ich pól
- Obsługa sygnatur typów (DW_FORM_ref_sig8)
- Wsparcie dla różnych architektur (16-bit, 32-bit, 64-bit)

## Architektura

Program jest podzielony na moduły odpowiedzialne za:

1. **FileDescriptor** - RAII wrapper dla deskryptorów plików
2. **dwarf_utils** - Narzędzia pomocnicze (konwersje, obsługa błędów)
3. **type_cache** - Cache sygnatur typów z sekcji .debug_types
4. **type_info** - Pobieranie nazw i rozmiarów typów
5. **die_processor** - Przetwarzanie DIE i traversal drzewa DWARF
6. **main** - Główna logika programu

## Licencja

[Tu dodaj informacje o licencji]

## Autor

[Tu dodaj informacje o autorze]
