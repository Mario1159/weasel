#pragma once

#include <entt/entt.hpp>


namespace wsl
{

namespace rsc
{

namespace io
{

/*! \brief Supported resource types that can be referenced in scenes and project metadata. */
enum class resource_type
{
  //! 3D model (glTF).
  model,
  //! 2D texture image (PNG, HDR, etc.).
  image,
  //! Cubemap texture (Skybox, etc.).
  cubemap,
  //! Another scene instance.
  scene,
  //! Audio asset (WAV, MP3, etc.).
  audio
};

/*!
 * \brief A generic reference to a resource, combining its type and unique identifier.
 */
struct resource_ref
{
  //! The type of the referenced resource.
  resource_type type;
  //! The unique identifier of the resource.
  entt::id_type id;
};

} // namespace io

} // namespace rsc

} // namespace wsl
