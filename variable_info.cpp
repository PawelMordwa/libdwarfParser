#include "variable_info.h"

#include <iomanip>
#include <iostream>

// Definicja globalnego wektora
std::vector<VariableInfo> g_variables;

// Funkcja pomocnicza do wyświetlania pojedynczego member z obsługą zagnieżdżenia
static void print_member(const VariableInfo& member, const std::string& indent_str,
						 bool is_last, int depth = 0)
{
	std::string prefix = is_last ? "     └─ " : "     ├─ ";

	std::cout << indent_str << prefix
			  << std::left << std::setw(18) << member.name
			  << "| Adres: 0x" << std::hex << std::setw(8) << member.address
			  << "| Typ: " << std::setw(25) << member.type
			  << "| Rozmiar: " << std::dec << member.size << " B" << std::endl;

	// Jeśli member ma własne pola (zagnieżdżona struktura), wyświetl je rekurencyjnie
	if (!member.members.empty())
	{
		// Kontynuuj wcięcie
		std::string continuation = is_last ? "        " : "     │  ";
		std::string nested_indent = indent_str + continuation;

		// Określ typ struktury
		std::string type_desc = "";
		if (member.is_union)
			type_desc = "unii";
		else if (member.is_class)
			type_desc = "klasy";
		else if (member.is_struct)
			type_desc = "struktury";

		if (!type_desc.empty())
		{
			std::cout << nested_indent << "└─ Pola " << type_desc << " ("
					  << member.members.size() << " elementów):" << std::endl;
		}

		// Wyświetl zagnieżdżone elementy
		for (size_t j = 0; j < member.members.size(); ++j)
		{
			bool nested_is_last = (j == member.members.size() - 1);
			print_member(member.members[j], nested_indent, nested_is_last, depth + 1);
		}
	}
}

// Funkcja pomocnicza do wyświetlania pojedynczej zmiennej
static void print_variable(const VariableInfo& var, int indent = 0)
{
	std::string indent_str(indent * 2, ' ');

	std::cout << indent_str << "Zmienna: " << std::left << std::setw(20) << var.name
			  << "| Adres: 0x" << std::hex << std::setw(8) << var.address
			  << "| Typ: " << std::setw(25) << var.type
			  << "| Rozmiar: " << std::dec << var.size << " B" << std::endl;

	// Jeśli ma pola (struct/union/class), wyświetl je z wcięciem
	if (!var.members.empty())
	{
		std::string type_desc = "";
		if (var.is_union)
			type_desc = "unii";
		else if (var.is_class)
			type_desc = "klasy";
		else if (var.is_struct)
			type_desc = "struktury";

		if (!type_desc.empty())
		{
			std::cout << indent_str << "  └─ Pola " << type_desc << " ("
					  << var.members.size() << " elementów):" << std::endl;
		}

		for (size_t i = 0; i < var.members.size(); ++i)
		{
			const auto& member = var.members[i];
			bool is_last = (i == var.members.size() - 1);

			// Użyj nowej funkcji do wyświetlania member z obsługą zagnieżdżenia
			print_member(member, indent_str, is_last, 0);
		}
		std::cout << std::endl;	 // Pusta linia po wyświetleniu wszystkich pól
	}
}

// Wyświetl wszystkie zebrane zmienne
void print_all_variables()
{
	std::cout << "\n=== Zebrane zmienne (łącznie: " << g_variables.size() << ") ===" << std::endl;
	std::cout << std::endl;

	for (const auto& var : g_variables)
	{
		print_variable(var);
	}
}

// Wyczyść globalną strukturę
void clear_variables()
{
	g_variables.clear();
}
