#pragma once

#include <cstdint>

namespace wsl
{

namespace phys
{

// Collision layer definitions and filtering logic.
namespace layers
{

/** Index of a collision layer. */
using layer_index_t = std::uint8_t;
/** Bit mask containing multiple collision layers. */
using layer_mask_t = std::uint16_t;

inline constexpr layer_index_t collision_layer_count = 8;
inline constexpr layer_mask_t all_collision_layers
    = (layer_mask_t{ 1U } << collision_layer_count) - 1U;

// Legacy object layers are kept for non-rigidbody physics objects like
// character controllers and area sensors.
inline constexpr std::uint16_t STATIC // NOLINT(readability-identifier-naming)
    = 0;
inline constexpr std::uint16_t dynamic = 1;
inline constexpr std::uint16_t character = 2;

inline constexpr std::uint16_t encoded_rigidbody_flag = 1U << 15;
inline constexpr std::uint16_t encoded_layer_bits = 3;
inline constexpr std::uint16_t encoded_layer_mask
    = (1U << encoded_layer_bits) - 1U;
inline constexpr std::uint16_t encoded_mask_shift = encoded_layer_bits;
inline constexpr std::uint16_t encoded_mask_bits = collision_layer_count;
inline constexpr std::uint16_t encoded_collision_mask
    = ((1U << encoded_mask_bits) - 1U) << encoded_mask_shift;
inline constexpr std::uint16_t encoded_motion_shift
    = encoded_mask_shift + encoded_mask_bits;
inline constexpr std::uint16_t encoded_motion_mask = 0x3U
                                                     << encoded_motion_shift;

enum class motion_bucket : std::uint8_t
{
  static_body = 0,
  moving_body = 1,
  character = 2
};

constexpr layer_index_t
clamp_layer_index (layer_index_t value)
{
  return value < collision_layer_count ? value : 0;
}

constexpr layer_mask_t
clamp_layer_mask (layer_mask_t value)
{
  return value & all_collision_layers;
}

constexpr layer_mask_t
bit_for_layer (layer_index_t value)
{
  return layer_mask_t{ 1U } << clamp_layer_index (value);
}

constexpr bool
is_encoded_rigidbody (std::uint16_t layer)
{
  return (layer & encoded_rigidbody_flag) != 0;
}

constexpr motion_bucket
get_motion_bucket (std::uint16_t layer)
{
  if (!is_encoded_rigidbody (layer)) {
    switch (layer) {
    case character:
      return motion_bucket::character;
    case dynamic:
      return motion_bucket::moving_body;
    case STATIC:
    default:
      return motion_bucket::static_body;
    }
  }

  return static_cast<motion_bucket> ((layer & encoded_motion_mask)
                                     >> encoded_motion_shift);
}

constexpr layer_index_t
get_collision_layer (std::uint16_t layer)
{
  if (!is_encoded_rigidbody (layer)) {
    return 0;
  }

  return clamp_layer_index (
      static_cast<layer_index_t> (layer & encoded_layer_mask));
}

constexpr layer_mask_t
get_collision_mask (std::uint16_t layer)
{
  if (!is_encoded_rigidbody (layer)) {
    return all_collision_layers;
  }

  return clamp_layer_mask (static_cast<layer_mask_t> (
      (layer & encoded_collision_mask) >> encoded_mask_shift));
}

constexpr std::uint16_t
make_rigidbody_object_layer (layer_index_t layer_index, layer_mask_t mask,
                             motion_bucket motion)
{
  return static_cast<std::uint16_t> (
      encoded_rigidbody_flag
      | static_cast<uint16_t> (clamp_layer_index (layer_index))
      | (static_cast<uint16_t> (clamp_layer_mask (mask)) << encoded_mask_shift)
      | (static_cast<uint16_t> (motion) << encoded_motion_shift));
}

constexpr bool
layer_mask_allows (std::uint16_t a, std::uint16_t b)
{
  return (get_collision_mask (a) & bit_for_layer (get_collision_layer (b)))
         != 0;
}

constexpr bool
rigidbodies_can_collide (std::uint16_t a, std::uint16_t b)
{
  return layer_mask_allows (a, b) && layer_mask_allows (b, a);
}

} // namespace layers

} // namespace phys

} // namespace wsl
