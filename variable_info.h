#ifndef VARIABLE_INFO_H
#define VARIABLE_INFO_H

#include <cstdint>
#include <string>
#include <vector>

// Struktura przechowująca informacje o pojedynczej zmiennej/DIE
struct VariableInfo
{
	std::string name;  // Nazwa zmiennej
	uint64_t address;  // Adres w pamięci
	std::string type;  // Pełna nazwa typu (np. "struct MyStruct", "int", "typedef MyType")
	uint64_t size;	   // Rozmiar w bajtach

	// Opcjonalne: dodatkowe informacje
	bool is_struct;	 // Czy to struktura
	bool is_union;	 // Czy to unia
	bool is_class;	 // Czy to klasa

	// Dla struktur/unii/klas - lista pól
	std::vector<VariableInfo> members;

	VariableInfo()
		: address(0), size(0), is_struct(false), is_union(false), is_class(false) {}
};

// Globalna struktura przechowująca wszystkie zmienne
extern std::vector<VariableInfo> g_variables;

// Funkcje pomocnicze
void print_all_variables();
void clear_variables();

#endif	// VARIABLE_INFO_H
