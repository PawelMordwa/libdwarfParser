#ifndef TYPE_INFO_H
#define TYPE_INFO_H

#include <dwarf.h>
#include <libdwarf.h>

#include <cstdint>
#include <string>

// Funkcje do pobierania informacji o typach
std::string get_type_name(Dwarf_Debug dbg, Dwarf_Die type_die,
						  bool from_cache = false);

Dwarf_Unsigned get_type_size(Dwarf_Debug dbg, Dwarf_Die type_die, bool& found,
							 bool from_cache = false);

// Nowe funkcje pomocnicze dla pełnej informacji o typie
std::string get_full_type_info(Dwarf_Debug dbg, Dwarf_Die variable_die);
uint64_t get_type_size_simple(Dwarf_Debug dbg, Dwarf_Die variable_die);

void print_type_info(Dwarf_Debug dbg, Dwarf_Die variable_die);

#endif	// TYPE_INFO_H
