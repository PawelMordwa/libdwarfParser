#ifndef DIE_PROCESSOR_H
#define DIE_PROCESSOR_H

#include <libdwarf-0/dwarf.h>
#include <libdwarf-0/libdwarf.h>

#include <cstdint>
#include <string>

// Funkcje do przetwarzania DIE (Debug Information Entry)
void process_struct_members(Dwarf_Debug dbg, Dwarf_Die struct_die,
							uint64_t base_address,
							const std::string& struct_name);

void process_die(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size);

void traverse_dies(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size);

#endif	// DIE_PROCESSOR_H
