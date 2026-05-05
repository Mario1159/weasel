#pragma once

#include <RmlUi/Core/SystemInterface.h>
#include <SDL3/SDL.h>

namespace editor
{

class ui_system_interface : public Rml::SystemInterface
{
public:
  double
  GetElapsedTime () override
  {
    return SDL_GetTicks () / 1000.0;
  }

  bool
  LogMessage (Rml::Log::Type type, const Rml::String &message) override
  {
    SDL_Log ("[RmlUi] %s", message.c_str ());
    return true;
  }
};

} // namespace editor
