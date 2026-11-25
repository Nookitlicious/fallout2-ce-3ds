#ifndef FALLOUT_SFALL_INI_H_
#define FALLOUT_SFALL_INI_H_

#include <cstddef>
#include "sfall_arrays.h"

namespace fallout {

/// Sets base directory to lookup .ini files.
void sfall_ini_set_base_path(const char* path);

/// Reads integer key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_int(const char* triplet, int* value);

/// Reads string key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_string(const char* triplet, char* value, size_t size);

/// Returns a temporary associative array of string keys/values, or an empty map.
ArrayId sfall_ini_get_section(Program* program, const char* fileName, const char* section);

/// Returns a temporary list of section names, or an empty list.
ArrayId sfall_ini_get_sections(Program* program, const char* fileName);

/// Writes integer key identified by "fileName|section|key" triplet.
bool sfall_ini_set_int(const char* triplet, int value);

/// Writes string key identified by "fileName|section|key" triplet.
bool sfall_ini_set_string(const char* triplet, const char* value);

} // namespace fallout

#endif /* FALLOUT_SFALL_INI_H_ */
