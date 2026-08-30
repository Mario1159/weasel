#pragma once

#if !defined(WSL_MODULE_BUILD)
#include <entt/entt.hpp>
#endif

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

/**
 * Sentinel stored in a resource id when it references nothing.
 *
 * Must match the sentinel used by the editor UI ("None" entries) and
 * by component serialization. Never use ``0``: default-constructed ids
 * used to initialize to zero, which made unset resources render as
 * ``(00000000)`` instead of "None".
 */
inline constexpr entt::id_type no_resource_id = entt::null;

/**
 * Maps the legacy zero sentinel found in older project files to
 * :cpp:member:`no_resource_id`.
 */
inline void
normalize_resource_id (entt::id_type &value)
{
  if (value == 0) {
    value = no_resource_id;
  }
}

/** Unique identifier for a 3D model resource. */
struct model_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const model_id &other) const
  {
    return value == other.value;
  }
  static void register_meta ();
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);
};

/** Unique identifier for an image resource. */
struct image_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const image_id &other) const
  {
    return value == other.value;
  }
  static void register_meta ();
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);
};

/** Unique identifier for a cubemap resource. */
struct cubemap_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const cubemap_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for a scene resource. */
struct scene_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const scene_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for an audio resource. */
struct audio_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const audio_id &other) const
  {
    return value == other.value;
  }
  static void register_meta ();
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);
};

/** Unique identifier for a UI layout resource. */
struct ui_layout_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const ui_layout_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for a font resource. */
struct font_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const font_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for a shader resource. */
struct shader_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const shader_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for a shader program resource. */
struct shader_program_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const shader_program_id &other) const
  {
    return value == other.value;
  }
};

/** Unique identifier for a material asset resource. */
struct material_id
{
  /** The hashed identifier value. */
  entt::id_type value{ no_resource_id };
  bool
  operator== (const material_id &other) const
  {
    return value == other.value;
  }
  static void register_meta ();
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime);
};

} // namespace rsc

} // namespace wsl

// ---------------------------------------------------------------------------
// cereal serialization helpers for resource id types
// ---------------------------------------------------------------------------

#if !defined(WSL_MODULE_BUILD)
#include <cereal/cereal.hpp>
#endif

namespace wsl
{

namespace rsc
{

template <class Archive>
void
serialize (Archive &ar, model_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, image_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, cubemap_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, scene_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, audio_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, ui_layout_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, font_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, shader_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, shader_program_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

template <class Archive>
void
serialize (Archive &ar, material_id &id)
{
  ar (cereal::make_nvp ("value", id.value));
  if constexpr (std::is_base_of_v<cereal::detail::InputArchiveBase, Archive>) {
    normalize_resource_id (id.value);
  }
}

} // namespace rsc

} // namespace wsl
