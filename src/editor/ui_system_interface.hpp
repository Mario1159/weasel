#pragma once

#include "wsl/log/log.hpp"
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>

namespace editor
{

class ui_system_interface : public SystemInterface_SDL
{
public:
  bool
  LogMessage (Rml::Log::Type /*type*/, const Rml::String &message) override
  {
    wsl::log::editor ()->debug ("{}", message);
    return true;
  }
};

} // namespace editor
