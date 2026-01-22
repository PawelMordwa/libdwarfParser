#include "type_cache.h"

#include <iomanip>
#include <iostream>

#include "dwarf_utils.h"

// Cache dla sygnatur typów (DWARF 4 .debug_types)
std::map<uint64_t, Dwarf_Die> type_signature_cache;

// Budowanie cache sygnatur typów z sekcji .debug_types
void build_type_signature_cache(Dwarf_Debug dbg)
{
	Dwarf_Error err;

	// Iteracja przez sekcję .debug_types (is_info = 0)
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

	int type_unit_count = 0;
	int loaded_count = 0;

	// Śledź offset bieżącej jednostki
	Dwarf_Unsigned current_cu_offset = 0;

	while (true)
	{
		int res = dwarf_next_cu_header_d(
			dbg, 0,	 // is_info = 0 dla .debug_types
			&cu_header_length, &version_stamp, &abbrev_offset, &address_size,
			&length_size, &extension_size, &type_signature, &type_offset,
			&next_cu_header, &header_cu_type, &err);

		if (res == DW_DLV_NO_ENTRY)
			break;
		if (res != DW_DLV_OK)
		{
			// Niektóre błędy są normalne pod koniec iteracji
			break;
		}

		type_unit_count++;

		// type_offset jest względem początku tej jednostki typu (po polu
		// initial_length) current_cu_offset pokazuje gdzie jesteśmy w sekcji
		// Globalny offset = current_cu_offset + type_offset

		Dwarf_Off global_type_offset = current_cu_offset + type_offset;
		Dwarf_Die type_die = nullptr;
		if (dwarf_offdie_b(dbg, global_type_offset, 0, &type_die, &err) ==
			DW_DLV_OK)
		{
			// Zapisz w cache: sygnatura -> DIE
			uint64_t sig_key = sig8_to_uint64(type_signature);
			type_signature_cache[sig_key] = type_die;
			loaded_count++;
			// Debug: wyświetl informacje o typie (tylko pierwsze 10 dla czytelności)
			if (loaded_count <= 10 || loaded_count % 20 == 0)
			{
				char* type_name = nullptr;
				Dwarf_Half tag;
				std::string debug_info = "";

				if (dwarf_tag(type_die, &tag, &err) == DW_DLV_OK)
				{
					const char* tag_name = "";
					switch (tag)
					{
						case DW_TAG_structure_type:
							tag_name = "struct";
							break;
						case DW_TAG_union_type:
							tag_name = "union";
							break;
						case DW_TAG_enumeration_type:
							tag_name = "enum";
							break;
						case DW_TAG_typedef:
							tag_name = "typedef";
							break;
						case DW_TAG_base_type:
							tag_name = "base";
							break;
						case DW_TAG_pointer_type:
							tag_name = "pointer";
							break;
						case DW_TAG_array_type:
							tag_name = "array";
							break;
						case DW_TAG_const_type:
							tag_name = "const";
							break;
						case DW_TAG_volatile_type:
							tag_name = "volatile";
							break;
						default:
							tag_name = "other";
							break;
					}
					debug_info = std::string(" [") + tag_name + "]";
				}

				if (dwarf_diename(type_die, &type_name, &err) == DW_DLV_OK)
				{
					std::cout << "  Typ #" << loaded_count << ": " << std::left
							  << std::setw(25) << type_name << debug_info << std::endl;
					dwarf_dealloc(dbg, type_name, DW_DLA_STRING);
				}
				else
				{
					std::cout << "  Typ #" << loaded_count << ": " << std::left
							  << std::setw(25) << "(bez nazwy)" << debug_info
							  << std::endl;
				}
			}
		}

		// Przejdź do następnej jednostki
		current_cu_offset = next_cu_header;
	}

	std::cout << "Znaleziono " << type_unit_count << " jednostek typów"
			  << std::endl;
	std::cout << "Załadowano " << loaded_count << " sygnatur typów do cache"
			  << std::endl;
}
