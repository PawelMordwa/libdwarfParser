#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Nagłówki libdwarf
#include <libdwarf-0/dwarf.h>
#include <libdwarf-0/libdwarf.h>

// Cache dla sygnatur typów (DWARF 4 .debug_types)
std::map<uint64_t, Dwarf_Die> type_signature_cache;

// Prosta klasa RAII do obsługi deskryptora pliku
class FileDescriptor
{
	int fd;

   public:
	FileDescriptor(const std::string& path)
	{
		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0)
		{
			throw std::runtime_error("Nie można otworzyć pliku: " + path);
		}
	}
	~FileDescriptor()
	{
		if (fd >= 0)
			close(fd);
	}
	int get() const { return fd; }
};

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

// Funkcja pomocnicza do pobierania nazwy typu (rekurencyjnie rozwiązuje
// kwalifikatory)
std::string get_type_name(Dwarf_Debug dbg, Dwarf_Die type_die,
						  bool from_cache = false)
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
							 bool from_cache = false)
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
			// Debug: wyświetl formę
			// std::cout << " [form=0x" << std::hex << form << std::dec << "]";

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

// Funkcja pomocnicza do przetwarzania pól struktury
void process_struct_members(Dwarf_Debug dbg, Dwarf_Die struct_die,
							uint64_t base_address,
							const std::string& struct_name)
{
	Dwarf_Error err;
	Dwarf_Die child;

	// Pobierz pierwsze dziecko (pierwsze pole struktury)
	if (dwarf_child(struct_die, &child, &err) != DW_DLV_OK)
	{
		return;
	}

	Dwarf_Die current = child;

	do
	{
		Dwarf_Half tag;
		if (dwarf_tag(current, &tag, &err) == DW_DLV_OK && tag == DW_TAG_member)
		{
			char* raw_member_name = nullptr;
			if (dwarf_diename(current, &raw_member_name, &err) == DW_DLV_OK)
			{
				std::string member_name(raw_member_name);

				// Pobierz offset pola w strukturze
				Dwarf_Attribute offset_attr;
				if (dwarf_attr(current, DW_AT_data_member_location, &offset_attr,
							   &err) == DW_DLV_OK)
				{
					Dwarf_Unsigned member_offset;
					if (dwarf_formudata(offset_attr, &member_offset, &err) == DW_DLV_OK)
					{
						uint64_t member_address = base_address + member_offset;

						std::cout << "  ├─ Pole: " << std::left << std::setw(18)
								  << member_name << "| Adres: 0x" << std::hex
								  << member_address;
						print_type_info(dbg, current);
						std::cout << std::endl;
					}
				}
				dwarf_dealloc(dbg, raw_member_name, DW_DLA_STRING);
			}
		}

		Dwarf_Die sibling;
		int res = dwarf_siblingof_b(dbg, current, 1, &sibling, &err);
		if (current != child)
		{
			dwarf_dealloc(dbg, current, DW_DLA_DIE);
		}
		if (res != DW_DLV_OK)
			break;
		current = sibling;

	} while (true);
}

void process_die(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size)
{
	Dwarf_Error err;
	Dwarf_Half tag;
	char* raw_name = nullptr;

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK)
		return;

	if (tag == DW_TAG_variable)
	{
		if (dwarf_diename(die, &raw_name, &err) == DW_DLV_OK)
		{
			std::string name(raw_name);	 // Konwersja na std::string

			Dwarf_Attribute loc_attr;
			if (dwarf_attr(die, DW_AT_location, &loc_attr, &err) == DW_DLV_OK)
			{
				Dwarf_Block* block;
				if (dwarf_formblock(loc_attr, &block, &err) == DW_DLV_OK)
				{
					// Bezpieczniejsze rzutowanie w C++
					auto* data_ptr = reinterpret_cast<unsigned char*>(block->bl_data);

					// Sprawdzamy opcode DW_OP_addr (0x03)
					if (block->bl_len > 0 && *data_ptr == 0x03)
					{
						uint64_t address = 0;
						// Sprawdź czy blok ma wystarczającą długość (opcode + address_size)
						if (block->bl_len >= (1 + address_size))
						{
							// Kopiowanie adresu z bufora bajtów - obsługa różnych rozmiarów
							// data_ptr + 1, ponieważ pierwszy bajt to opcode
							switch (address_size)
							{
								case 2:	 // 16-bit (C2000, starsze procesory)
									address = *reinterpret_cast<uint16_t*>(data_ptr + 1);
									break;
								case 4:	 // 32-bit
									address = *reinterpret_cast<uint32_t*>(data_ptr + 1);
									break;
								case 8:	 // 64-bit
									address = *reinterpret_cast<uint64_t*>(data_ptr + 1);
									break;
								default:
									// Dla niestandardowych rozmiarów - manualne kopiowanie
									for (size_t i = 0; i < address_size && i < 8; i++)
									{
										address |= (uint64_t)data_ptr[1 + i] << (i * 8);
									}
									break;
							}

							std::cout << "Zmienna: " << std::left << std::setw(20) << name
									  << "| Adres: 0x" << std::hex << address;
							print_type_info(dbg, die);
							std::cout << std::endl;

							// Sprawdź czy to struktura i wyświetl jej pola
							Dwarf_Attribute type_attr;
							if (dwarf_attr(die, DW_AT_type, &type_attr, &err) == DW_DLV_OK)
							{
								Dwarf_Off type_offset;
								Dwarf_Bool is_info = true;

								// Spróbuj dwarf_formref() najpierw
								int res =
									dwarf_formref(type_attr, &type_offset, &is_info, &err);
								if (res != DW_DLV_OK)
								{
									res = dwarf_global_formref(type_attr, &type_offset, &err);
									if (res == DW_DLV_OK)
										is_info = true;
								}

								if (res == DW_DLV_OK)
								{
									Dwarf_Die type_die;
									if (dwarf_offdie_b(dbg, type_offset, is_info, &type_die,
													   &err) == DW_DLV_OK)
									{
										Dwarf_Half type_tag;
										if (dwarf_tag(type_die, &type_tag, &err) == DW_DLV_OK)
										{
											// Obsługa typedef - rozwiń do rzeczywistego typu
											while (type_tag == DW_TAG_typedef ||
												   type_tag == DW_TAG_const_type ||
												   type_tag == DW_TAG_volatile_type)
											{
												Dwarf_Attribute base_type_attr;
												if (dwarf_attr(type_die, DW_AT_type, &base_type_attr,
															   &err) == DW_DLV_OK)
												{
													Dwarf_Off base_type_offset;
													Dwarf_Bool base_is_info = true;

													// Spróbuj dwarf_formref() najpierw
													int base_res =
														dwarf_formref(base_type_attr, &base_type_offset,
																	  &base_is_info, &err);
													if (base_res != DW_DLV_OK)
													{
														base_res = dwarf_global_formref(
															base_type_attr, &base_type_offset, &err);
														if (base_res == DW_DLV_OK)
															base_is_info = true;
													}

													if (base_res == DW_DLV_OK)
													{
														Dwarf_Die base_type_die;
														if (dwarf_offdie_b(dbg, base_type_offset,
																		   base_is_info, &base_type_die,
																		   &err) == DW_DLV_OK)
														{
															dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
															type_die = base_type_die;
															dwarf_tag(type_die, &type_tag, &err);
														}
														else
															break;
													}
													else
														break;
												}
												else
													break;
											}

											if (type_tag == DW_TAG_structure_type)
											{
												process_struct_members(dbg, type_die, address, name);
											}
										}
										dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
									}
								}  // Zamknięcie if (res == DW_DLV_OK)
							}
						}
					}
				}
			}
			dwarf_dealloc(dbg, raw_name, DW_DLA_STRING);
		}
	}
}

// Rekurencja
void traverse_dies(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half address_size)
{
	Dwarf_Error err;
	Dwarf_Die child;

	process_die(dbg, die, address_size);

	if (dwarf_child(die, &child, &err) == DW_DLV_OK)
	{
		traverse_dies(dbg, child, address_size);
	}

	Dwarf_Die sibling;
	// Używamy dwarf_siblingof_b (is_info = 1)
	if (dwarf_siblingof_b(dbg, die, 1, &sibling, &err) == DW_DLV_OK)
	{
		dwarf_dealloc(dbg, die, DW_DLA_DIE);
		traverse_dies(dbg, sibling, address_size);
	}
}

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