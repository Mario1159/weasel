#include "runtime_project_module_api.hpp"
#include <vector>

namespace wsl
{

namespace reg
{
namespace runtime
{

namespace
{

comp::singl::runtime_context *g_active_runtime_context = nullptr;

}

comp::singl::runtime_context *
active_runtime_context ()
{
  return g_active_runtime_context;
}

// Implementations for runtime_registrar declared in the header.

void
runtime_registrar::set_active_runtime_context (comp::singl::runtime_context *runtime_ctx)
{
  g_active_runtime_context = runtime_ctx;
}

std::vector<runtime_registrar::registration_fn> &
runtime_registrar::component_registrations ()
{
  static std::vector<runtime_registrar::registration_fn> regs;
  return regs;
}

std::vector<runtime_registrar::registration_fn> &
runtime_registrar::singleton_registrations ()
{
  static std::vector<runtime_registrar::registration_fn> regs;
  return regs;
}

std::vector<runtime_registrar::registration_fn> &
runtime_registrar::system_registrations ()
{
  static std::vector<runtime_registrar::registration_fn> regs;
  return regs;
}

} // namespace runtime
} // namespace reg

} // namespace wsl
