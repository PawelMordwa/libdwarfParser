#ifndef DWARF_UTILS_H
#define DWARF_UTILS_H

#include <dwarf.h>
#include <libdwarf.h>

#include <cstdint>
#include <string>

// Funkcje pomocnicze do obsługi błędów i konwersji
void check_error(int res, Dwarf_Error err, const std::string& msg);
uint64_t sig8_to_uint64(const Dwarf_Sig8& sig);

#endif	// DWARF_UTILS_H
