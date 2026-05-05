#pragma once

// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
// clang-format on

#include <cstdint>


namespace wsl
{

namespace phys
{

/**
 * @namespace wsl::phys::layers
 * @brief Collision layer definitions and filtering logic.
 */
namespace layers
{

using layer_index_t = uint8_t;
using layer_mask_t = uint16_t;

static constexpr layer_index_t collision_layer_count = 8;
static constexpr layer_mask_t all_collision_layers
    = (layer_mask_t{ 1U } << collision_layer_count) - 1U;

// Legacy object layers are kept for non-rigidbody physics objects like
// character controllers and area sensors.
static constexpr JPH::ObjectLayer STATIC = 0;
static constexpr JPH::ObjectLayer dynamic = 1;
static constexpr JPH::ObjectLayer character = 2;

static constexpr JPH::ObjectLayer encoded_rigidbody_flag = 1U << 15;
static constexpr uint16_t encoded_layer_bits = 3;
static constexpr uint16_t encoded_layer_mask
    = (1U << encoded_layer_bits) - 1U;
static constexpr uint16_t encoded_mask_shift = encoded_layer_bits;
static constexpr uint16_t encoded_mask_bits = collision_layer_count;
static constexpr uint16_t encoded_collision_mask
    = ((1U << encoded_mask_bits) - 1U) << encoded_mask_shift;
static constexpr uint16_t encoded_motion_shift
    = encoded_mask_shift + encoded_mask_bits;
static constexpr uint16_t encoded_motion_mask = 0x3U << encoded_motion_shift;

enum class motion_bucket : uint16_t
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
is_encoded_rigidbody (JPH::ObjectLayer layer)
{
  return (layer & encoded_rigidbody_flag) != 0;
}

constexpr motion_bucket
get_motion_bucket (JPH::ObjectLayer layer)
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
get_collision_layer (JPH::ObjectLayer layer)
{
  if (!is_encoded_rigidbody (layer)) {
    return 0;
  }

  return clamp_layer_index (
      static_cast<layer_index_t> (layer & encoded_layer_mask));
}

constexpr layer_mask_t
get_collision_mask (JPH::ObjectLayer layer)
{
  if (!is_encoded_rigidbody (layer)) {
    return all_collision_layers;
  }

  return clamp_layer_mask (
      static_cast<layer_mask_t> ((layer & encoded_collision_mask)
                                 >> encoded_mask_shift));
}

constexpr JPH::ObjectLayer
make_rigidbody_object_layer (layer_index_t layer_index, layer_mask_t mask,
                             motion_bucket motion)
{
  return static_cast<JPH::ObjectLayer> (
      encoded_rigidbody_flag
      | static_cast<uint16_t> (clamp_layer_index (layer_index))
      | (static_cast<uint16_t> (clamp_layer_mask (mask)) << encoded_mask_shift)
      | (static_cast<uint16_t> (motion) << encoded_motion_shift));
}

constexpr bool
layer_mask_allows (JPH::ObjectLayer a, JPH::ObjectLayer b)
{
  return (get_collision_mask (a) & bit_for_layer (get_collision_layer (b)))
         != 0;
}

constexpr bool
rigidbodies_can_collide (JPH::ObjectLayer a, JPH::ObjectLayer b)
{
  return layer_mask_allows (a, b) && layer_mask_allows (b, a);
}

} // namespace layers

} // namespace phys

} // namespace wsl
