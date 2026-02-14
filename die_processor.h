#ifndef DIE_PROCESSOR_H
#define DIE_PROCESSOR_H

#include <dwarf.h>
#include <libdwarf.h>

#include <cstdint>
#include <string>
#include <vector>

// Forward declaration
struct VariableInfo;

// Funkcje do przetwarzania DIE (Debug Information Entry)
void process_struct_members(Dwarf_Debug dbg, Dwarf_Die struct_die,
							uint64_t base_address,
							const std::string& struct_name,
							std::vector<VariableInfo>* members_list = nullptr);

void process_union_members(Dwarf_Debug dbg, Dwarf_Die union_die,
						   uint64_t base_address,
						   const std::string& union_name,
						   std::vector<VariableInfo>* members_list = nullptr);

void process_class_members(Dwarf_Debug dbg, Dwarf_Die class_die,
						   uint64_t base_address,
						   const std::string& class_name,
						   std::vector<VariableInfo>* members_list = nullptr);

void process_die(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size);

void traverse_dies(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size);

#endif	// DIE_PROCESSOR_H
