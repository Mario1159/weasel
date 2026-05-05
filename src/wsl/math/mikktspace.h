#pragma once

// Compatibility wrapper to preserve historic include name "mikktspace.h".
// It forwards to the project's canonical mikktspace_header and provides
// lightweight macro aliases for older camelCase identifiers used by the
// original C implementation sources.

#include "mikktspace_header"

// Compatibility aliases (old -> new)
#ifndef MIKKTSPACE_COMPAT_ALIASES
#define MIKKTSPACE_COMPAT_ALIASES

// Member name alias
#define m_pInterface m_p_interface

// Function name aliases
#define genTangSpaceDefault gen_tang_space_default
#define genTangSpace gen_tang_space

#endif // MIKKTSPACE_COMPAT_ALIASES
