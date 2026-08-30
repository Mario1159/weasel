module;

#define WSL_MODULE_BUILD

import "thirdparty/thirdparty_all.hpp";
import "phys/jolt_all.hpp";
import "das/daScript_all.hpp";

export module wsl.reg;

export {
#include "component_registry.hpp"
#include "registry_handle.hpp"
#include "registry_queries.hpp"
#include "runtime_project_module.hpp"
#include "singleton_registry.hpp"
#include "system_factory_registry.hpp"
}
