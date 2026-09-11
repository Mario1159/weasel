#pragma once

#include "das_engine.hpp"

namespace das
{
class Module;
class ModuleGroup;
}

namespace wsl::das
{

/**
 * Registers low-level interop functions with the weasel_api module.
 *
 * These use addInterop instead of addExtern, providing:
 * - "Any type" arguments (vec4f template parameter)
 * - Runtime TypeInfo inspection via call->types[i]
 * - Call-site debug info via call->debugInfo
 *
 * Functions registered:
 * - describe_type(value) — returns type name, size, and struct fields
 * - type_name(value) — returns the type name string
 * - type_size(value) — returns sizeof for the value's type
 * - struct_field_count(value) — returns field count for struct values
 * - struct_field_names(value) — returns comma-separated field names
 * - call_site_info() — returns source file:line of the caller
 */
void register_interop_functions (::das::Module &mod);

} // namespace wsl::das
