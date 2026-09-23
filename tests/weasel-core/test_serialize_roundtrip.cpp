// Part of weasel_core_tests; the doctest main lives in
// test_event_bus.cpp.
#include "doctest.h"

#include "wsl/gfx/material_asset.hpp"
#include "wsl/gfx/shader_graph.hpp"
#include "wsl/serialize/serialize.hpp"

#include <variant>

// Regression test for the cereal -> rfl material migration: the editor
// writes .wslmat files with tagged variants (rfl::AddTagsToVariants) and
// resource_manager::load (material_id) reads them back with the same
// dialect. Every alternative of material_parameter::value_type must
// survive the round trip without drifting to a different type (the
// default index-tagged variant encoding turns int into float and
// cubemap_id into image_id).
TEST_CASE ("material_asset round-trips through rfl with stable variant tags")
{
  wsl::gfx::material_asset mat;
  mat.name = "roundtrip";
  mat.path = "materials/roundtrip.wslmat";
  mat.shader_program.value = 1234;
  mat.vertex_shader_path = "engine://compiled_shaders/custom.vert.slang.spv";
  mat.double_sided = true;
  mat.alpha_test = false;

  using wsl::gfx::material_parameter;
  mat.default_parameters["u_Float"]
      = material_parameter ("u_Float", 0.5f);
  mat.default_parameters["u_Int"] = material_parameter ("u_Int", 42);
  mat.default_parameters["u_Bool"] = material_parameter ("u_Bool", true);
  mat.default_parameters["u_Vec3"]
      = material_parameter ("u_Vec3", glm::vec3{ 1.0f, 2.0f, 3.0f });
  mat.default_parameters["u_Tex"]
      = material_parameter ("u_Tex", wsl::rsc::image_id{ 77 });
  mat.default_parameters["u_Cube"]
      = material_parameter ("u_Cube", wsl::rsc::cubemap_id{ 88 });

  std::string write_error;
  std::string const json
      = wsl::serialize::json_write_p<rfl::AddTagsToVariants> (mat,
                                                               &write_error);
  REQUIRE (json.empty () == false);

  wsl::gfx::material_asset back;
  std::string read_error;
  REQUIRE (wsl::serialize::json_read_p<wsl::gfx::material_asset,
                                       rfl::AddTagsToVariants> (
              json, back, &read_error));

  CHECK (back.name == "roundtrip");
  CHECK (back.shader_program.value == 1234);
  CHECK (back.vertex_shader_path
         == "engine://compiled_shaders/custom.vert.slang.spv");
  CHECK (back.double_sided == true);
  CHECK (back.alpha_test == false);
  REQUIRE (back.default_parameters.size ()
           == mat.default_parameters.size ());

  auto const &fl = back.default_parameters.at ("u_Float").value;
  REQUIRE (std::holds_alternative<float> (fl));
  CHECK (std::get<float> (fl) == doctest::Approx (0.5f));

  auto const &i = back.default_parameters.at ("u_Int").value;
  REQUIRE (std::holds_alternative<int> (i)); // must not drift to float
  CHECK (std::get<int> (i) == 42);

  auto const &b = back.default_parameters.at ("u_Bool").value;
  REQUIRE (std::holds_alternative<bool> (b));
  CHECK (std::get<bool> (b) == true);

  auto const &v3 = back.default_parameters.at ("u_Vec3").value;
  REQUIRE (std::holds_alternative<glm::vec3> (v3));
  CHECK (std::get<glm::vec3> (v3).x == doctest::Approx (1.0f));
  CHECK (std::get<glm::vec3> (v3).z == doctest::Approx (3.0f));

  auto const &tex = back.default_parameters.at ("u_Tex").value;
  REQUIRE (std::holds_alternative<wsl::rsc::image_id> (tex)); // not cubemap
  CHECK (std::get<wsl::rsc::image_id> (tex).value == 77);

  auto const &cube = back.default_parameters.at ("u_Cube").value;
  REQUIRE (std::holds_alternative<wsl::rsc::cubemap_id> (cube));
  CHECK (std::get<wsl::rsc::cubemap_id> (cube).value == 88);
}

// The shader graph lost its cereal serialize() methods in the same
// migration; save_graph/load_graph now rely purely on rfl reflection.
TEST_CASE ("shader_graph round-trips through rfl")
{
  wsl::gfx::shader_graph graph;
  graph.name = "roundtrip_graph";

  wsl::gfx::graph_node node{};
  node.id = 7;
  node.name = "Float";
  node.kind = wsl::gfx::graph_node_kind::uniform_float;
  node.pos_x = 12.5F;
  node.pos_y = -3.0F;
  node.properties["name"] = "u_Float7";
  node.properties["default"] = "0.25";

  wsl::gfx::graph_pin pin{};
  pin.id = 700;
  pin.name = "Value";
  pin.type = wsl::gfx::graph_pin_type::float_scalar;
  pin.is_input = false;
  node.pins.push_back (pin);
  graph.nodes.push_back (node);

  wsl::gfx::graph_link link{};
  link.from_node = 7;
  link.from_pin = 700;
  link.to_node = 1;
  link.to_pin = 101;
  graph.links.push_back (link);

  std::string write_error;
  std::string const json = wsl::serialize::json_write (graph, &write_error);
  REQUIRE (json.empty () == false);

  wsl::gfx::shader_graph back;
  std::string read_error;
  REQUIRE (wsl::serialize::json_read (json, back, &read_error));

  CHECK (back.name == "roundtrip_graph");
  REQUIRE (back.nodes.size () == 1);
  CHECK (back.nodes[0].id == 7);
  CHECK (back.nodes[0].name == "Float");
  CHECK (back.nodes[0].kind == wsl::gfx::graph_node_kind::uniform_float);
  CHECK (back.nodes[0].pos_x == doctest::Approx (12.5F));
  CHECK (back.nodes[0].pos_y == doctest::Approx (-3.0F));
  REQUIRE (back.nodes[0].properties.size () == 2);
  CHECK (back.nodes[0].properties.at ("name") == "u_Float7");
  CHECK (back.nodes[0].properties.at ("default") == "0.25");
  REQUIRE (back.nodes[0].pins.size () == 1);
  CHECK (back.nodes[0].pins[0].id == 700);
  CHECK (back.nodes[0].pins[0].type == wsl::gfx::graph_pin_type::float_scalar);
  CHECK (back.nodes[0].pins[0].is_input == false);
  REQUIRE (back.links.size () == 1);
  CHECK (back.links[0].from_node == 7);
  CHECK (back.links[0].from_pin == 700);
  CHECK (back.links[0].to_node == 1);
  CHECK (back.links[0].to_pin == 101);
}
