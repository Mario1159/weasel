#pragma once

// Clang may skip emitting linkable symbols from a dedicated implementation TU
// for this single-header library. Including the inline implementation at the
// call sites avoids the missing-symbol path entirely.
#define ImLengthSqr ImViewGuizmo_ImLengthSqr
#define IMVIEWGUIZMO_IMPLEMENTATION
#include <ImViewGuizmo.h>

namespace editor
{
#undef ImLengthSqr

} // namespace editor
