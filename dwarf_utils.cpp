#include "dwarf_utils.h"

#include <cstdlib>
#include <iostream>

void check_error(int res, Dwarf_Error err, const std::string& msg)
{
	if (res == DW_DLV_ERROR)
	{
		std::cerr << "Błąd: " << msg << " - " << dwarf_errmsg(err) << std::endl;
		exit(1);
	}
}

// Konwersja Dwarf_Sig8 do uint64_t
uint64_t sig8_to_uint64(const Dwarf_Sig8& sig)
{
	uint64_t result = 0;
	for (int i = 0; i < 8; i++)
	{
		result |= ((uint64_t)sig.signature[i]) << (i * 8);
	}
	return result;
}
