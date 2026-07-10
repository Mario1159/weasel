#include "material_asset.hpp"
#include "wsl/log/log.hpp"

#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace wsl
{

namespace gfx
{

material_parameter::value_type
material_instance::get_parameter (const std::string &name,
                                  const material_asset &asset) const
{
  auto it = overrides.find (name);
  if (it != overrides.end ()) {
    return it->second.value;
  }
  auto def = asset.default_parameters.find (name);
  if (def != asset.default_parameters.end ()) {
    return def->second.value;
  }
  return float (0.0f); // safe fallback
}

std::vector<uint8_t>
material_instance::build_uniform_blob (const shader_reflection &reflection,
                                       const material_asset &asset) const
{
  // Find the Material UBO (b0) and pack parameters into it.
  const shader_uniform_buffer *ubo = nullptr;
  for (const auto &ub : reflection.uniform_buffers) {
    if (ub.binding == 0) {
      ubo = &ub;
      break;
    }
  }

  if (ubo == nullptr || ubo->size == 0) {
    return {};
  }

  std::vector<uint8_t> blob (ubo->size, 0);

  for (const auto &member : ubo->members) {
    auto val = get_parameter (member.name, asset);
    // Write the value at the reflected offset. We use a simple
    // std::visit to handle each variant type.
    std::visit (
        [&] (auto &&v) {
          using T = std::decay_t<decltype (v)>;
          if constexpr (std::is_same_v<T, float>) {
            if (member.offset + sizeof (float) <= blob.size ()) {
              std::memcpy (blob.data () + member.offset, &v, sizeof (v));
            }
          } else if constexpr (std::is_same_v<T, glm::vec2>) {
            if (member.offset + sizeof (glm::vec2) <= blob.size ()) {
              std::memcpy (blob.data () + member.offset, glm::value_ptr (v),
                           sizeof (v));
            }
          } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (member.offset + sizeof (glm::vec3) <= blob.size ()) {
              std::memcpy (blob.data () + member.offset, glm::value_ptr (v),
                           sizeof (v));
            }
          } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (member.offset + sizeof (glm::vec4) <= blob.size ()) {
              std::memcpy (blob.data () + member.offset, glm::value_ptr (v),
                           sizeof (v));
            }
          } else if constexpr (std::is_same_v<T, int>) {
            if (member.offset + sizeof (int) <= blob.size ()) {
              std::memcpy (blob.data () + member.offset, &v, sizeof (v));
            }
          } else if constexpr (std::is_same_v<T, bool>) {
            if (member.offset + sizeof (int) <= blob.size ()) {
              int iv = v ? 1 : 0;
              std::memcpy (blob.data () + member.offset, &iv, sizeof (iv));
            }
          }
          // image_id / cubemap_id are not written into the UBO.
        },
        val);
  }

  return blob;
}

} // namespace gfx

} // namespace wsl
