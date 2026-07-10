#pragma once

#include "shader_program.hpp"
#include <string>
#include <vector>

namespace wsl
{

namespace gfx
{

/*!
 * \brief Runtime Slang compiler wrapper.
 *
 * Compiles Slang source strings to platform-native bytecode at runtime
 * using the already-linked slang::slang library.
 */
class shader_compiler
{
public:
  shader_compiler ();
  ~shader_compiler ();

  /*! \brief Compile a vertex shader from Slang source. */
  bool compile_vertex (const char *source, size_t source_len,
                       shader_program &out_program);

  /*! \brief Compile a fragment shader from Slang source. */
  bool compile_fragment (const char *source, size_t source_len,
                         shader_program &out_program);

  /*! \brief Fill reflection metadata for an already-compiled program. */
  bool reflect (shader_program &program);

  /*! \brief Returns the last diagnostic string if compilation failed. */
  const std::string &
  last_error () const
  {
    return m_last_error;
  }

private:
  std::string m_last_error;

  bool init ();
  bool compile_stage (const char *source, size_t source_len,
                      SDL_GPUShaderStage stage, shader_program &out);
  static SDL_GPUShaderFormat native_format ();
};

} // namespace gfx

} // namespace wsl
