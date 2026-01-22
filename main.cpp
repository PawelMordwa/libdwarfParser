#include <libdwarf-0/dwarf.h>
#include <libdwarf-0/libdwarf.h>

#include <iostream>

#include "die_processor.h"
#include "dwarf_utils.h"
#include "file_descriptor.h"
#include "type_cache.h"
#include "type_info.h"

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "Użycie: " << argv[0] << " <plik_elf>" << std::endl;
		return 1;
	}

	try
	{
		// RAII dla pliku
		FileDescriptor file(argv[1]);

		Dwarf_Debug dbg = nullptr;
		Dwarf_Error err;

		// Inicjalizacja (API 0.11.1)
		if (dwarf_init_b(file.get(), DW_GROUPNUMBER_ANY, nullptr, nullptr, &dbg,
						 &err) != DW_DLV_OK)
		{
			std::cerr << "Błąd inicjalizacji DWARF: " << dwarf_errmsg(err)
					  << std::endl;
			return 1;
		}

		// Buduj cache sygnatur typów z .debug_types
		std::cout << "=== Budowanie cache sygnatur typów ===" << std::endl;
		build_type_signature_cache(dbg);
		std::cout << "========================================" << std::endl
				  << std::endl;

		// Zmienne dla nagłówka CU
		Dwarf_Unsigned cu_header_length;
		Dwarf_Half version_stamp;
		Dwarf_Off abbrev_offset;
		Dwarf_Half address_size;
		Dwarf_Half length_size;
		Dwarf_Half extension_size;
		Dwarf_Sig8 type_signature;
		Dwarf_Unsigned type_offset;
		Dwarf_Unsigned next_cu_header;
		Dwarf_Half header_cu_type;

		while (true)
		{
			int res = dwarf_next_cu_header_d(
				dbg, 1, &cu_header_length, &version_stamp, &abbrev_offset,
				&address_size, &length_size, &extension_size, &type_signature,
				&type_offset, &next_cu_header, &header_cu_type, &err);

			if (res == DW_DLV_NO_ENTRY)
				break;
			if (res != DW_DLV_OK)
			{
				std::cerr << "Błąd odczytu CU" << std::endl;
				break;
			}

			// Wyświetl informacje o architekturze (tylko raz)
			static bool first_cu = true;
			if (first_cu)
			{
				std::cout << "=== Informacje o architekturze ===" << std::endl;
				std::cout << "Rozmiar adresu: " << std::dec << (int)address_size
						  << " bajtów (" << (address_size * 8) << "-bit)" << std::endl;
				std::cout << "Wersja DWARF: " << version_stamp << std::endl;
				std::cout << "===================================" << std::endl
						  << std::endl;
				first_cu = false;
			}

			Dwarf_Die cu_die = nullptr;
			// Pobranie pierwszego DIE
			if (dwarf_siblingof_b(dbg, nullptr, 1, &cu_die, &err) == DW_DLV_OK)
			{
				traverse_dies(dbg, cu_die, address_size);
			}
		}

		// Zwolnij DIE z cache przed zamknięciem
		for (auto& pair : type_signature_cache)
		{
			dwarf_dealloc(dbg, pair.second, DW_DLA_DIE);
		}
		type_signature_cache.clear();

		dwarf_finish(dbg);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Wyjątek: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
