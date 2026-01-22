#include "type_info.h"

#include <iomanip>
#include <iostream>

#include "dwarf_utils.h"
#include "type_cache.h"

// Funkcja pomocnicza do pobierania nazwy typu (rekurencyjnie rozwiązuje
// kwalifikatory)
std::string get_type_name(Dwarf_Debug dbg, Dwarf_Die type_die,
						  bool from_cache)
{
	Dwarf_Error err;
	char* raw_type_name = nullptr;
	std::string type_name = "";
	std::string prefix = "";

	Dwarf_Half tag;
	if (dwarf_tag(type_die, &tag, &err) != DW_DLV_OK)
	{
		return "(nieznany)";
	}

	// Obsługa kwalifikatorów typu - zbieraj prefiksy i kontynuuj do typu bazowego
	bool is_qualifier = true;
	while (is_qualifier)
	{
		switch (tag)
		{
			case DW_TAG_const_type:
				prefix = "const " + prefix;
				break;
			case DW_TAG_volatile_type:
				prefix = "volatile " + prefix;
				break;
			case DW_TAG_restrict_type:
				prefix = "restrict " + prefix;
				break;
			case DW_TAG_pointer_type:
				// Dla wskaźników możemy też podążać dalej
				prefix = prefix + "*";
				break;
			default:
				is_qualifier = false;
				break;
		}

		if (is_qualifier)
		{
			// Pobierz referencję do typu bazowego
			Dwarf_Attribute base_type_attr;
			if (dwarf_attr(type_die, DW_AT_type, &base_type_attr, &err) ==
				DW_DLV_OK)
			{
				Dwarf_Off base_type_offset;
				Dwarf_Bool is_info = true;

				// Sprawdź formę atrybutu - dla sygnatur użyj cache
				Dwarf_Half form;
				if (dwarf_whatform(base_type_attr, &form, &err) == DW_DLV_OK &&
					form == DW_FORM_ref_sig8)
				{
					// To jest sygnatura - szukaj w cache
					Dwarf_Sig8 signature;
					if (dwarf_formsig8(base_type_attr, &signature, &err) == DW_DLV_OK)
					{
						uint64_t sig_key = sig8_to_uint64(signature);
						auto it = type_signature_cache.find(sig_key);
						if (it != type_signature_cache.end())
						{
							type_die = it->second;
							from_cache = true;
							if (dwarf_tag(type_die, &tag, &err) != DW_DLV_OK)
							{
								return prefix + "(nieznany)";
							}
							continue;  // Kontynuuj z nowym DIE
						}
					}
					// Jeśli nie znaleziono w cache
					break;
				}

				// Spróbuj dwarf_formref() najpierw
				int res =
					dwarf_formref(base_type_attr, &base_type_offset, &is_info, &err);
				if (res != DW_DLV_OK)
				{
					res = dwarf_global_formref(base_type_attr, &base_type_offset, &err);
					if (res == DW_DLV_OK)
						is_info = true;
				}

				if (res == DW_DLV_OK)
				{
					Dwarf_Die base_type_die;
					if (dwarf_offdie_b(dbg, base_type_offset, is_info, &base_type_die,
									   &err) == DW_DLV_OK)
					{
						// Zwolnij poprzedni DIE jeśli to nie jest oryginalny i nie z cache
						if (!from_cache)
						{
							// type_die będzie zwolniony przez wywołującego
						}
						type_die = base_type_die;
						if (dwarf_tag(type_die, &tag, &err) != DW_DLV_OK)
						{
							return prefix + "(nieznany)";
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				// Kwalifikator bez typu bazowego (np. void*)
				break;
			}
		}
	}

	// Teraz pobierz nazwę właściwego typu bazowego
	if (dwarf_diename(type_die, &raw_type_name, &err) == DW_DLV_OK)
	{
		type_name = std::string(raw_type_name);
		dwarf_dealloc(dbg, raw_type_name, DW_DLA_STRING);
	}
	else
	{
		// Jeśli nadal brak nazwy, sprawdź tag
		switch (tag)
		{
			case DW_TAG_pointer_type:
				type_name = "void*";
				break;
			case DW_TAG_array_type:
				type_name = "(tablica)";
				break;
			case DW_TAG_structure_type:
				type_name = "(struct)";
				break;
			case DW_TAG_union_type:
				type_name = "(union)";
				break;
			case DW_TAG_enumeration_type:
				type_name = "(enum)";
				break;
			default:
				type_name = "(nieznany)";
		}
	}

	return prefix + type_name;
}

// Funkcja pomocnicza do pobierania rozmiaru typu (podąża za kwalifikatorami i
// typedef)
Dwarf_Unsigned get_type_size(Dwarf_Debug dbg, Dwarf_Die type_die, bool& found,
							 bool from_cache)
{
	Dwarf_Error err;
	Dwarf_Unsigned size = 0;
	found = false;

	Dwarf_Die current_die = type_die;
	bool should_dealloc = false;

	// Podążaj za łańcuchem referencji typów
	while (true)
	{
		Dwarf_Half tag;
		if (dwarf_tag(current_die, &tag, &err) != DW_DLV_OK)
		{
			break;
		}

		// Sprawdź czy ten DIE ma informację o rozmiarze
		Dwarf_Attribute size_attr;
		if (dwarf_attr(current_die, DW_AT_byte_size, &size_attr, &err) ==
			DW_DLV_OK)
		{
			if (dwarf_formudata(size_attr, &size, &err) == DW_DLV_OK)
			{
				found = true;
				if (should_dealloc)
				{
					dwarf_dealloc(dbg, current_die, DW_DLA_DIE);
				}
				return size;
			}
		}

		// Jeśli to kwalifikator/typedef/wskaźnik - idź głębiej
		if (tag == DW_TAG_typedef || tag == DW_TAG_const_type ||
			tag == DW_TAG_volatile_type || tag == DW_TAG_restrict_type ||
			tag == DW_TAG_pointer_type)
		{
			Dwarf_Attribute base_type_attr;
			if (dwarf_attr(current_die, DW_AT_type, &base_type_attr, &err) ==
				DW_DLV_OK)
			{
				Dwarf_Off base_type_offset;
				Dwarf_Bool is_info = true;

				// Sprawdź formę atrybutu - dla sygnatur użyj cache
				Dwarf_Half form;
				if (dwarf_whatform(base_type_attr, &form, &err) == DW_DLV_OK &&
					form == DW_FORM_ref_sig8)
				{
					// To jest sygnatura - szukaj w cache
					Dwarf_Sig8 signature;
					if (dwarf_formsig8(base_type_attr, &signature, &err) == DW_DLV_OK)
					{
						uint64_t sig_key = sig8_to_uint64(signature);
						auto it = type_signature_cache.find(sig_key);
						if (it != type_signature_cache.end())
						{
							if (should_dealloc && !from_cache)
							{
								dwarf_dealloc(dbg, current_die, DW_DLA_DIE);
							}
							current_die = it->second;
							should_dealloc = false;	 // NIE zwalniaj - jest w cache
							from_cache = true;
							continue;
						}
					}
					// Jeśli nie znaleziono w cache, przerwij
					break;
				}

				// Spróbuj dwarf_formref() najpierw
				int res =
					dwarf_formref(base_type_attr, &base_type_offset, &is_info, &err);
				if (res != DW_DLV_OK)
				{
					res = dwarf_global_formref(base_type_attr, &base_type_offset, &err);
					if (res == DW_DLV_OK)
						is_info = true;
				}

				if (res == DW_DLV_OK)
				{
					Dwarf_Die base_type_die;
					// Jeśli from_cache, prawdopodobnie szukamy w .debug_types
					Dwarf_Bool search_is_info = from_cache ? 0 : is_info;
					if (dwarf_offdie_b(dbg, base_type_offset, search_is_info,
									   &base_type_die, &err) == DW_DLV_OK)
					{
						if (should_dealloc && !from_cache)
						{
							dwarf_dealloc(dbg, current_die, DW_DLA_DIE);
						}
						current_die = base_type_die;
						should_dealloc = !from_cache;  // Zwalniaj tylko jeśli nie z cache
						continue;					   // Kontynuuj pętlę z nowym DIE
					}
				}
			}
		}

		// Nie udało się znaleźć rozmiaru
		break;
	}

	if (should_dealloc)
	{
		dwarf_dealloc(dbg, current_die, DW_DLA_DIE);
	}

	return size;
}

// Funkcja pomocnicza do pobierania informacji o typie (nazwa i rozmiar)
void print_type_info(Dwarf_Debug dbg, Dwarf_Die variable_die)
{
	Dwarf_Error err;
	Dwarf_Attribute type_attr;
	Dwarf_Die type_die = nullptr;

	// Pobierz atrybut typu
	if (dwarf_attr(variable_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK)
	{
		// Sprawdź jaką formę ma ten atrybut (dla debugowania)
		Dwarf_Half form;
		if (dwarf_whatform(type_attr, &form, &err) == DW_DLV_OK)
		{
			Dwarf_Off offset = 0;
			Dwarf_Bool is_info = true;
			int res = DW_DLV_ERROR;

			// Różne metody w zależności od formy
			switch (form)
			{
				case DW_FORM_ref1:
				case DW_FORM_ref2:
				case DW_FORM_ref4:
				case DW_FORM_ref8:
				case DW_FORM_ref_udata:
					// Lokalna referencja - potrzebujemy offsetu CU
					res = dwarf_formref(type_attr, &offset, &is_info, &err);
					if (res == DW_DLV_OK)
					{
						Dwarf_Off cu_offset = 0;
						if (dwarf_CU_dieoffset_given_die(variable_die, &cu_offset, &err) ==
							DW_DLV_OK)
						{
							offset = cu_offset + offset;
						}
					}
					break;

				case DW_FORM_ref_addr:
					// Globalna referencja
					res = dwarf_global_formref(type_attr, &offset, &err);
					is_info = true;
					break;

				case DW_FORM_ref_sig8:
					// Sygnatura typu (DWARF 4+) - używane przez TI CGT dla C2000
					{
						Dwarf_Sig8 signature;
						if (dwarf_formsig8(type_attr, &signature, &err) == DW_DLV_OK)
						{
							uint64_t sig_key = sig8_to_uint64(signature);

							// Szukaj w cache
							auto it = type_signature_cache.find(sig_key);
							if (it != type_signature_cache.end())
							{
								type_die = it->second;

								// Pobierz nazwę typu z cache
								std::string type_name = get_type_name(dbg, type_die, true);
								std::cout << " | Typ: " << std::left << std::setw(17)
										  << type_name;

								// Pobierz rozmiar typu z cache
								bool found = false;
								Dwarf_Unsigned size = get_type_size(dbg, type_die, found, true);

								if (found)
								{
									std::cout << " | Rozmiar: " << std::dec << size << " bajtów";
								}
								else
								{
									std::cout << " | Rozmiar: (brak informacji)";
								}

								// NIE zwalniaj type_die - jest w cache!
								return;
							}
							else
							{
								std::cout << " | Typ: (sygnatura nie znaleziona w cache 0x"
										  << std::hex << sig_key << std::dec << ")";
								std::cout << " | Rozmiar: (nieznany)";
								return;
							}
						}
						else
						{
							std::cout << " | Typ: (błąd odczytu sygnatury)";
							return;
						}
					}

				default:
					// Inne formy - spróbuj uniwersalnie
					res = dwarf_formref(type_attr, &offset, &is_info, &err);
					if (res != DW_DLV_OK)
					{
						res = dwarf_global_formref(type_attr, &offset, &err);
						if (res == DW_DLV_OK)
							is_info = true;
					}
					break;
			}

			if (res == DW_DLV_OK)
			{
				if (dwarf_offdie_b(dbg, offset, is_info, &type_die, &err) ==
					DW_DLV_OK)
				{
					// Pobierz nazwę typu
					std::string type_name = get_type_name(dbg, type_die);
					std::cout << " | Typ: " << std::left << std::setw(17) << type_name;

					// Pobierz rozmiar typu (rekurencyjnie)
					bool found = false;
					Dwarf_Unsigned size = get_type_size(dbg, type_die, found);

					if (found)
					{
						std::cout << " | Rozmiar: " << std::dec << size << " bajtów";
					}
					else
					{
						std::cout << " | Rozmiar: (brak informacji)";
					}

					dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
				}
				else
				{
					std::cout << " | Typ: (błąd offdie, form=0x" << std::hex << form
							  << ", off=0x" << offset << ")";
				}
			}
			else
			{
				std::cout << " | Typ: (błąd ref, form=0x" << std::hex << form << ")";
			}
		}
		else
		{
			std::cout << " | Typ: (błąd whatform)";
		}
	}
	else
	{
		std::cout << " | Typ: (brak atrybutu)";
	}
}
