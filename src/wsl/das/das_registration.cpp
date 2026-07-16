#include "das_registration.hpp"
#include "das_engine.hpp"
#include "das_system.hpp"
#include "../log/log.hpp"

#include <vector>

namespace wsl::das
{

namespace
{

struct pending_das_registration
{
  std::string script_path;
  das_engine *engine;
};

std::vector<pending_das_registration> &
get_pending_registrations ()
{
  static std::vector<pending_das_registration> registrations;
  return registrations;
}

} // anonymous namespace

void
register_das_file (const std::string &script_path, das_engine &engine,
                   reg::runtime::runtime_module_registration_context &ctx)
{
  // Store the registration for later execution during finalize_load()
  get_pending_registrations ().push_back ({ script_path, &engine });

  wsl::log::cmake ()->debug ("Registered daslang file: {}", script_path);
}

void
clear_das_registrations ()
{
  get_pending_registrations ().clear ();
}

} // namespace wsl::das
