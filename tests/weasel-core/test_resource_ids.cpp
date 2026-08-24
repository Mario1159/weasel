// Part of weasel_core_tests; the doctest main lives in
// test_event_bus.cpp.
#include "doctest.h"

#include "wsl/rsc/resource_ids.hpp"

using namespace wsl::rsc;

// Regression test: unset resources must use the entt::null sentinel.
// They used to default to zero, which the editor treated as a set id
// and rendered as "(00000000)" instead of matching the combo's "None"
// entry.
TEST_CASE ("resource ids default to the no-resource sentinel")
{
  CHECK (model_id{}.value == no_resource_id);
  CHECK (material_id{}.value == no_resource_id);
  CHECK (image_id{}.value == no_resource_id);
  CHECK (cubemap_id{}.value == no_resource_id);
  CHECK (scene_id{}.value == no_resource_id);
  CHECK (audio_id{}.value == no_resource_id);
  CHECK (ui_layout_id{}.value == no_resource_id);
  CHECK (font_id{}.value == no_resource_id);
  CHECK (shader_id{}.value == no_resource_id);
  CHECK (shader_program_id{}.value == no_resource_id);
}

TEST_CASE ("normalize_resource_id maps legacy zero to the sentinel")
{
  entt::id_type legacy = 0;
  normalize_resource_id (legacy);
  CHECK (legacy == no_resource_id);

  entt::id_type real = entt::hashed_string::value ("builtin/cube");
  if (real == 0) {
    // Hash collisions with zero are theoretically possible; pick
    // another value so the test stays meaningful.
    real = 42;
  }
  normalize_resource_id (real);
  CHECK (real != no_resource_id);
}
