#include "das_ecs_binds.hpp"

#include "daScript/ast/ast.h"
#include "daScript/daScriptModule.h"

namespace wsl::das
{

class Module_Ecs : public ::das::Module
{
public:
  Module_Ecs () : Module ("weasel_ecs")
  {
    ::das::ModuleLibrary lib (this);
    lib.addBuiltInModule ();
  }

  virtual ::das::ModuleAotType
  aotRequire (::das::TextWriter &tw) const override
  {
    tw << "#include \"wsl/das/das_ecs_binds.hpp\"\n";
    return ::das::ModuleAotType::cpp;
  }
};

static ::das::Module *g_ecs_module = nullptr;

void
register_ecs_module (::das::ModuleGroup &module_group)
{
  g_ecs_module = new Module_Ecs ();
  module_group.addModule (g_ecs_module);
}

::das::Module *
get_ecs_module ()
{
  return g_ecs_module;
}

::das::Module *
create_worker_ecs_module ()
{
  return new Module_Ecs ();
}

} // namespace wsl::das
