#pragma once

// Compatibility wrapper to preserve historic include name "mikktspace.h".
// It forwards to the project's canonical mikktspace_header and provides
// lightweight macro aliases for older camelCase identifiers used by the
// original C implementation sources.

#include "mikktspace_header"

// Compatibility aliases (old -> new)
#ifndef MIKKTSPACE_COMPAT_ALIASES
#define MIKKTSPACE_COMPAT_ALIASES

// Member name alias so the original C source can use m_pInterface while
// the canonical C++ header uses m_p_interface.
#define m_pInterface m_p_interface

// NOTE: The function name aliases (genTangSpaceDefault -> gen_tang_space_default,
// genTangSpace -> gen_tang_space) have been removed so the original C
// implementation keeps its camelCase names and the C++ wrapper in
// mikktspace_cpp_impl.cpp provides the snake_case public API without
// causing duplicate-symbol / infinite-recursion issues.

#endif // MIKKTSPACE_COMPAT_ALIASES
