#ifndef TYPE_INFO_H
#define TYPE_INFO_H

#include <libdwarf-0/dwarf.h>
#include <libdwarf-0/libdwarf.h>

#include <string>

// Funkcje do pobierania informacji o typach
std::string get_type_name(Dwarf_Debug dbg, Dwarf_Die type_die,
						  bool from_cache = false);

Dwarf_Unsigned get_type_size(Dwarf_Debug dbg, Dwarf_Die type_die, bool& found,
							 bool from_cache = false);

void print_type_info(Dwarf_Debug dbg, Dwarf_Die variable_die);

#endif	// TYPE_INFO_H
