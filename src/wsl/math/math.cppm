module;

#define WSL_MODULE_BUILD

import "thirdparty/thirdparty_all.hpp";
import "phys/jolt_all.hpp";
import "das/daScript_all.hpp";

export module wsl.math;

export {
#include "matrix.hpp"
#include "vector.hpp"
#include "mikktspace.hpp"
}
