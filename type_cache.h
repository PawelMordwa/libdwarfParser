#ifndef TYPE_CACHE_H
#define TYPE_CACHE_H

#include <libdwarf-0/dwarf.h>
#include <libdwarf-0/libdwarf.h>

#include <cstdint>
#include <map>

// Cache dla sygnatur typów (DWARF 4 .debug_types)
extern std::map<uint64_t, Dwarf_Die> type_signature_cache;

// Budowanie cache sygnatur typów z sekcji .debug_types
void build_type_signature_cache(Dwarf_Debug dbg);

#endif	// TYPE_CACHE_H
