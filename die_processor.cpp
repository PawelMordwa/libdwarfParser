#include "die_processor.h"

#include <iomanip>
#include <iostream>

#include "type_info.h"

// Funkcja pomocnicza do przetwarzania pól struktury
void process_struct_members(Dwarf_Debug dbg, Dwarf_Die struct_die,
							uint64_t base_address,
							const std::string& struct_name [[maybe_unused]])
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
						if (block->bl_len >= static_cast<Dwarf_Unsigned>(1 + address_size))
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
								}
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
