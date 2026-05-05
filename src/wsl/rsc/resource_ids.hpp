#pragma once

#include <entt/entt.hpp>


namespace wsl
{

namespace comp
{
namespace singl
{
class runtime_context;
}
}

namespace rsc
{

/*!
 * \brief Unique identifier for a 3D model resource.
 */
struct model_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const model_id &other) const { return value == other.value; }
  static void register_meta ();
  bool custom_inspect (const char *label, comp::singl::runtime_context *runtime);
};

/*!
 * \brief Unique identifier for an image resource.
 */
struct image_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const image_id &other) const { return value == other.value; }
};

/*!
 * \brief Unique identifier for a cubemap resource.
 */
struct cubemap_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const cubemap_id &other) const { return value == other.value; }
};

/*!
 * \brief Unique identifier for a scene resource.
 */
struct scene_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const scene_id &other) const { return value == other.value; }
};

/*!
 * \brief Unique identifier for an audio resource.
 */
struct audio_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const audio_id &other) const { return value == other.value; }
  static void register_meta ();
  bool custom_inspect (const char *label, comp::singl::runtime_context *runtime);
};

/*!
 * \brief Unique identifier for a UI layout resource.
 */
struct ui_layout_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const ui_layout_id &other) const { return value == other.value; }
};

/*!
 * \brief Unique identifier for a font resource.
 */
struct font_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const font_id &other) const { return value == other.value; }
};

/*!
 * \brief Unique identifier for a shader resource.
 */
struct shader_id
{
  //! The hashed identifier value.
  entt::id_type value{ 0 };
  bool operator== (const shader_id &other) const { return value == other.value; }
};

} // namespace rsc

} // namespace wsl
