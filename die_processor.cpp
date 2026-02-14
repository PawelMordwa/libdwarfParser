#include "die_processor.h"

#include <iomanip>
#include <iostream>

#include "dwarf_utils.h"
#include "type_cache.h"
#include "type_info.h"
#include "variable_info.h"

// Funkcja pomocnicza do przetwarzania pól struktury
void process_struct_members(Dwarf_Debug dbg, Dwarf_Die struct_die,
							uint64_t base_address,
							const std::string& struct_name [[maybe_unused]],
							std::vector<VariableInfo>* members_list)
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

						// Twórz obiekt VariableInfo dla pola
						VariableInfo member_info;
						member_info.name = member_name;
						member_info.address = member_address;
						member_info.type = get_full_type_info(dbg, current);
						member_info.size = get_type_size_simple(dbg, current);

						if (members_list)
						{
							members_list->push_back(member_info);
						}
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

// Funkcja do przetwarzania pól unii
void process_union_members(Dwarf_Debug dbg, Dwarf_Die union_die,
						   uint64_t base_address,
						   const std::string& union_name [[maybe_unused]],
						   std::vector<VariableInfo>* members_list)
{
	Dwarf_Error err;
	Dwarf_Die child;

	// Pobierz pierwsze dziecko (pierwsze pole unii)
	if (dwarf_child(union_die, &child, &err) != DW_DLV_OK)
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

				// W unii wszystkie pola mają ten sam adres (offset 0)
				// ale sprawdzamy atrybut dla spójności z DWARF
				Dwarf_Attribute offset_attr;
				uint64_t member_offset = 0;

				if (dwarf_attr(current, DW_AT_data_member_location, &offset_attr,
							   &err) == DW_DLV_OK)
				{
					Dwarf_Unsigned offset;
					if (dwarf_formudata(offset_attr, &offset, &err) == DW_DLV_OK)
					{
						member_offset = offset;
					}
				}

				uint64_t member_address = base_address + member_offset;

				// Twórz obiekt VariableInfo dla pola unii
				VariableInfo member_info;
				member_info.name = member_name;
				member_info.address = member_address;
				member_info.type = get_full_type_info(dbg, current);
				member_info.size = get_type_size_simple(dbg, current);

				if (members_list)
				{
					members_list->push_back(member_info);
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

// Funkcja do przetwarzania składowych klasy (C++)
void process_class_members(Dwarf_Debug dbg, Dwarf_Die class_die,
						   uint64_t base_address,
						   const std::string& class_name [[maybe_unused]],
						   std::vector<VariableInfo>* members_list)
{
	Dwarf_Error err;
	Dwarf_Die child;

	// Pobierz pierwsze dziecko (pierwsze pole klasy)
	if (dwarf_child(class_die, &child, &err) != DW_DLV_OK)
	{
		return;
	}

	Dwarf_Die current = child;

	do
	{
		Dwarf_Half tag;
		if (dwarf_tag(current, &tag, &err) != DW_DLV_OK)
		{
			// Przejdź do następnego
			Dwarf_Die sibling;
			int res = dwarf_siblingof_b(dbg, current, 1, &sibling, &err);
			if (current != child)
				dwarf_dealloc(dbg, current, DW_DLA_DIE);
			if (res != DW_DLV_OK)
				break;
			current = sibling;
			continue;
		}

		// Obsługa dziedziczenia (klasa bazowa)
		if (tag == DW_TAG_inheritance)
		{
			// Pobierz offset klasy bazowej
			Dwarf_Attribute offset_attr;
			if (dwarf_attr(current, DW_AT_data_member_location, &offset_attr, &err) == DW_DLV_OK)
			{
				Dwarf_Unsigned base_offset = 0;
				int formudata_result = dwarf_formudata(offset_attr, &base_offset, &err);

				if (formudata_result != DW_DLV_OK)
				{
					Dwarf_Block* loc_block;
					if (dwarf_formblock(offset_attr, &loc_block, &err) == DW_DLV_OK)
					{
						auto* data = reinterpret_cast<unsigned char*>(loc_block->bl_data);
						if (loc_block->bl_len >= 2 && data[0] == 0x23)
						{
							base_offset = data[1];
							formudata_result = DW_DLV_OK;
						}
					}
				}

				if (formudata_result == DW_DLV_OK)
				{
					uint64_t base_class_address = base_address + base_offset;

					// Pobierz DIE typu klasy bazowej
					Dwarf_Attribute type_attr;
					if (dwarf_attr(current, DW_AT_type, &type_attr, &err) == DW_DLV_OK)
					{
						Dwarf_Die base_type_die = nullptr;
						Dwarf_Half form;

						if (dwarf_whatform(type_attr, &form, &err) == DW_DLV_OK)
						{
							if (form == DW_FORM_ref_sig8)
							{
								Dwarf_Sig8 signature;
								if (dwarf_formsig8(type_attr, &signature, &err) == DW_DLV_OK)
								{
									uint64_t sig_key = sig8_to_uint64(signature);
									auto it = type_signature_cache.find(sig_key);
									if (it != type_signature_cache.end())
									{
										base_type_die = it->second;
									}
								}
							}
							else
							{
								Dwarf_Off offset;
								Dwarf_Bool is_info = true;
								if (dwarf_formref(type_attr, &offset, &is_info, &err) == DW_DLV_OK ||
									dwarf_global_formref(type_attr, &offset, &err) == DW_DLV_OK)
								{
									dwarf_offdie_b(dbg, offset, is_info, &base_type_die, &err);
								}
							}

							// Rekurencyjnie zbierz pola klasy bazowej
							if (base_type_die != nullptr)
							{
								process_class_members(dbg, base_type_die, base_class_address,
													  class_name + "::base", members_list);

								// Zwolnij tylko jeśli nie z cache
								if (form != DW_FORM_ref_sig8)
								{
									dwarf_dealloc(dbg, base_type_die, DW_DLA_DIE);
								}
							}
						}
					}
				}
			}
		}

		// Przetwarzaj tylko pola danych (DW_TAG_member)
		// Pomijamy metody (DW_TAG_subprogram) i klasy zagnieżdżone
		if (tag == DW_TAG_member)
		{
			char* raw_member_name = nullptr;
			if (dwarf_diename(current, &raw_member_name, &err) == DW_DLV_OK)
			{
				std::string member_name(raw_member_name);
				uint64_t member_address = base_address;
				bool has_location = false;

				// Pobierz offset pola w klasie (jeśli istnieje)
				Dwarf_Attribute offset_attr;
				if (dwarf_attr(current, DW_AT_data_member_location, &offset_attr, &err) == DW_DLV_OK)
				{
					Dwarf_Unsigned member_offset = 0;

					// Spróbuj najpierw dwarf_formudata (dla prostych offsetów)
					int formudata_result = dwarf_formudata(offset_attr, &member_offset, &err);

					// Jeśli dwarf_formudata nie zadziałało, spróbuj jako blok (starsza konwencja DWARF)
					if (formudata_result != DW_DLV_OK)
					{
						Dwarf_Block* loc_block;
						if (dwarf_formblock(offset_attr, &loc_block, &err) == DW_DLV_OK)
						{
							// Blok zawiera wyrażenie lokalizacji - najczęściej DW_OP_plus_uconst + offset
							auto* data = reinterpret_cast<unsigned char*>(loc_block->bl_data);
							if (loc_block->bl_len >= 2 && data[0] == 0x23)	// DW_OP_plus_uconst
							{
								// Dekoduj ULEB128 (uproszczone dla małych wartości)
								member_offset = data[1];
								formudata_result = DW_DLV_OK;
							}
						}
					}

					if (formudata_result == DW_DLV_OK)
					{
						member_address = base_address + member_offset;
						has_location = true;
					}
				}

				// Kontynuuj przetwarzanie nawet jeśli nie ma lokalizacji
				// (dla static const members)
				// Dla static members spróbuj odczytać DW_AT_location (globalny adres)
				if (!has_location)
				{
					Dwarf_Attribute location_attr;
					if (dwarf_attr(current, DW_AT_location, &location_attr, &err) == DW_DLV_OK)
					{
						Dwarf_Unsigned static_addr = 0;
						// Spróbuj jako adres bezpośredni
						if (dwarf_formaddr(location_attr, &static_addr, &err) == DW_DLV_OK)
						{
							member_address = static_addr;
							has_location = true;
						}
						else
						{
							// Spróbuj jako blok wyrażenia lokalizacji
							Dwarf_Block* loc_block;
							if (dwarf_formblock(location_attr, &loc_block, &err) == DW_DLV_OK)
							{
								auto* data = reinterpret_cast<unsigned char*>(loc_block->bl_data);
								// DW_OP_addr (0x03) + 4-byte address dla TI C2000
								if (loc_block->bl_len >= 5 && data[0] == 0x03)
								{
									// Odczytaj 32-bitowy adres (little-endian)
									static_addr = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
									member_address = static_addr;
									has_location = true;
								}
							}
						}
					}
				}

				// Sprawdź modyfikator dostępu (public/private/protected)
				std::string access = "";
				Dwarf_Attribute access_attr;
				if (dwarf_attr(current, DW_AT_accessibility, &access_attr,
							   &err) == DW_DLV_OK)
				{
					Dwarf_Unsigned access_code;
					if (dwarf_formudata(access_attr, &access_code, &err) == DW_DLV_OK)
					{
						switch (access_code)
						{
							case DW_ACCESS_public:
								access = "public";
								break;
							case DW_ACCESS_protected:
								access = "protected";
								break;
							case DW_ACCESS_private:
								access = "private";
								break;
						}
					}
				}

				// Twórz obiekt VariableInfo dla pola klasy
				VariableInfo member_info;
				member_info.name = member_name;
				// Dla static members użyj base_address (początek struktury)
				member_info.address = member_address;

				// Dla static members dodaj oznaczenie
				std::string type_prefix = "";
				if (!has_location)
				{
					// Sprawdź czy to static member
					Dwarf_Attribute static_attr;
					if (dwarf_attr(current, DW_AT_external, &static_attr, &err) == DW_DLV_OK ||
						dwarf_attr(current, DW_AT_declaration, &static_attr, &err) == DW_DLV_OK)
					{
						type_prefix = "static ";
					}
				}

				member_info.type = (access.empty() ? "" : "[" + access + "] ") +
								   type_prefix + get_full_type_info(dbg, current);

				// Rozmiar: dla static members też oblicz prawdziwy rozmiar typu
				member_info.size = get_type_size_simple(dbg, current);

				// Sprawdź czy typ tego członka to struktura/klasa/unia
				// i jeśli tak, zbierz rekurencyjnie jego pola (tylko dla non-static)
				if (has_location)
				{
					Dwarf_Attribute type_attr;
					if (dwarf_attr(current, DW_AT_type, &type_attr, &err) == DW_DLV_OK)
					{
						Dwarf_Die type_die = nullptr;

						// Pobierz DIE typu
						Dwarf_Half form;
						if (dwarf_whatform(type_attr, &form, &err) == DW_DLV_OK)
						{
							if (form == DW_FORM_ref_sig8)
							{
								// Typ przez sygnaturę
								Dwarf_Sig8 signature;
								if (dwarf_formsig8(type_attr, &signature, &err) == DW_DLV_OK)
								{
									uint64_t sig_key = sig8_to_uint64(signature);
									auto it = type_signature_cache.find(sig_key);
									if (it != type_signature_cache.end())
									{
										type_die = it->second;
									}
								}
							}
							else
							{
								// Typ przez offset
								Dwarf_Off offset;
								Dwarf_Bool is_info = true;
								if (dwarf_formref(type_attr, &offset, &is_info, &err) == DW_DLV_OK ||
									dwarf_global_formref(type_attr, &offset, &err) == DW_DLV_OK)
								{
									dwarf_offdie_b(dbg, offset, is_info, &type_die, &err);
								}
							}

							// Jeśli mamy type_die, sprawdź czy to struktura/klasa/unia
							if (type_die != nullptr)
							{
								Dwarf_Half type_tag;
								if (dwarf_tag(type_die, &type_tag, &err) == DW_DLV_OK)
								{
									// Sprawdź czy to struktura/klasa/unia
									if (type_tag == DW_TAG_structure_type ||
										type_tag == DW_TAG_class_type ||
										type_tag == DW_TAG_union_type)
									{
										// Ustaw flagi
										member_info.is_struct = (type_tag == DW_TAG_structure_type);
										member_info.is_class = (type_tag == DW_TAG_class_type);
										member_info.is_union = (type_tag == DW_TAG_union_type);

										// Rekurencyjnie zbierz pola zagnieżdżonej struktury
										process_class_members(dbg, type_die, member_address,
															  member_name, &member_info.members);
									}
								}

								// Zwolnij type_die tylko jeśli nie jest z cache
								if (form != DW_FORM_ref_sig8)
								{
									dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
								}
							}
						}
					}
				}

				if (members_list)
				{
					members_list->push_back(member_info);
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

							// Utwórz obiekt VariableInfo dla zmiennej
							VariableInfo var_info;
							var_info.name = name;
							var_info.address = address;
							var_info.type = get_full_type_info(dbg, die);
							var_info.size = get_type_size_simple(dbg, die);

							// Sprawdź czy to struktura/unia/klasa i przetwórz jej pola
							Dwarf_Attribute type_attr;
							if (dwarf_attr(die, DW_AT_type, &type_attr, &err) == DW_DLV_OK)
							{
								Dwarf_Half form;
								if (dwarf_whatform(type_attr, &form, &err) == DW_DLV_OK)
								{
									// Obsługa DW_FORM_ref_sig8 - sygnatura typu (DWARF 4)
									if (form == DW_FORM_ref_sig8)
									{
										Dwarf_Sig8 signature;
										if (dwarf_formsig8(type_attr, &signature, &err) == DW_DLV_OK)
										{
											uint64_t sig_key = sig8_to_uint64(signature);

											auto it = type_signature_cache.find(sig_key);
											if (it != type_signature_cache.end())
											{
												Dwarf_Die type_die = it->second;
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
															// Sprawdź formę bazowego typu
															Dwarf_Half base_form;
															if (dwarf_whatform(base_type_attr, &base_form, &err) == DW_DLV_OK)
															{
																if (base_form == DW_FORM_ref_sig8)
																{
																	// Bazowy typ to kolejna sygnatura
																	Dwarf_Sig8 base_signature;
																	if (dwarf_formsig8(base_type_attr, &base_signature, &err) == DW_DLV_OK)
																	{
																		uint64_t base_sig_key = sig8_to_uint64(base_signature);
																		auto base_it = type_signature_cache.find(base_sig_key);
																		if (base_it != type_signature_cache.end())
																		{
																			type_die = base_it->second;
																			dwarf_tag(type_die, &type_tag, &err);
																		}
																		else
																			break;
																	}
																	else
																		break;
																}
																else
																{
																	// Bazowy typ to offset
																	Dwarf_Off base_type_offset;
																	Dwarf_Bool base_is_info = true;

																	int base_res = dwarf_formref(base_type_attr, &base_type_offset,
																								 &base_is_info, &err);
																	if (base_res != DW_DLV_OK)
																	{
																		base_res = dwarf_global_formref(base_type_attr, &base_type_offset, &err);
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
																			type_die = base_type_die;
																			dwarf_tag(type_die, &type_tag, &err);
																		}
																		else
																			break;
																	}
																	else
																		break;
																}
															}
															else
																break;
														}
														else
															break;
													}

													// Przetwarzaj typy złożone
													if (type_tag == DW_TAG_structure_type || type_tag == DW_TAG_class_type)
													{
														var_info.is_struct = true;
														process_class_members(dbg, type_die, address, name, &var_info.members);
													}
													else if (type_tag == DW_TAG_union_type)
													{
														var_info.is_union = true;
														process_union_members(dbg, type_die, address, name, &var_info.members);
													}
												}
											}
										}
									}
									else
									{
										// Obsługa innych form referencji (nie DW_FORM_ref_sig8)
										Dwarf_Off type_offset;
										Dwarf_Bool is_info = true;

										// Spróbuj dwarf_formref() najpierw
										int res = dwarf_formref(type_attr, &type_offset, &is_info, &err);
										if (res != DW_DLV_OK)
										{
											res = dwarf_global_formref(type_attr, &type_offset, &err);
											if (res == DW_DLV_OK)
												is_info = true;
										}

										if (res == DW_DLV_OK)
										{
											Dwarf_Die type_die;
											if (dwarf_offdie_b(dbg, type_offset, is_info, &type_die, &err) == DW_DLV_OK)
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
														if (dwarf_attr(type_die, DW_AT_type, &base_type_attr, &err) == DW_DLV_OK)
														{
															Dwarf_Off base_type_offset;
															Dwarf_Bool base_is_info = true;

															int base_res = dwarf_formref(base_type_attr, &base_type_offset,
																						 &base_is_info, &err);
															if (base_res != DW_DLV_OK)
															{
																base_res = dwarf_global_formref(base_type_attr, &base_type_offset, &err);
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

													// Przetwarzaj różne typy złożone
													if (type_tag == DW_TAG_structure_type || type_tag == DW_TAG_class_type)
													{
														var_info.is_struct = true;
														process_class_members(dbg, type_die, address, name, &var_info.members);
													}
													else if (type_tag == DW_TAG_union_type)
													{
														var_info.is_union = true;
														process_union_members(dbg, type_die, address, name, &var_info.members);
													}
												}
												dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
											}
										}
									}
								}
							}

							// Dodaj zmienną do globalnej struktury
							g_variables.push_back(var_info);
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
