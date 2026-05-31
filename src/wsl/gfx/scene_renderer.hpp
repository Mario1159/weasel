#pragma once

#include "wsl/gfx/render_window.hpp"
#include "cubemap.hpp"
#include "lighting.hpp"
#include "mesh.hpp"
#include "model_3d.hpp"
#include "render_context.hpp"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_pixels.h>

#include <entt/entity/fwd.hpp>

#include <array>
#include <cstddef>
#include <deque>
#include <vector>

namespace wsl
{

namespace rsc
{
class resource_manager;
}

namespace gfx
{

/*!
 * \brief Engine-facing 3D renderer.
 *
 * Owns GPU pipelines, per-frame submission state, and supporting passes such
 * as shadows, SSAO, skybox rendering, and outlines.
 */
class scene_renderer
{
public:
  /*!
   * \brief Camera state resolved for a single 3D frame.
   */
  struct view_state
  {
    //! True when a valid camera was resolved and the matrices can be used.
    bool valid = false;
    //! Aspect ratio used to build `proj`.
    float aspect_ratio = 1.0F;
    //! Camera origin in world space.
    glm::vec3 world_position{ 0.0F };
    //! World-to-view transform.
    glm::mat4 view{ 1.0F };
    //! View-to-clip transform.
    glm::mat4 proj{ 1.0F };
    //! Cached `proj * view`.
    glm::mat4 view_proj{ 1.0F };
  };

  /*!
   * \brief Single model submission reused by renderer passes.
   */
  struct draw_command
  {
    //! Model resource to draw.
    gfx::model_3d *model = nullptr;
    //! Scene slot inside the model resource.
    size_t scene_index = 0;
    //! Model-to-world transform.
    glm::mat4 transform{ 1.0F };
    //! Source entity for selection and debug linkage.
    entt::entity entity = entt::null;
    //! True when the submission should also receive an outline pass.
    bool draw_outline = false;
  };

  /*!
   * \brief Creates the renderer and its long-lived GPU resources.
   */
  scene_renderer (wsl::gfx::render_window &window, render_context *ctx,
                  wsl::rsc::resource_manager *res_mgr);

  /*!
   * \brief Releases GPU resources owned by the renderer.
   */
  ~scene_renderer ();

  //! Begins a frame with resolved camera data.
  void begin_frame (const view_state &view);
  //! Clears transient frame-local state.
  void end_frame ();
  //! Returns true when the renderer currently owns valid frame state.
  [[nodiscard]] auto has_active_frame () const -> bool;
  //! Returns the active frame view state.
  [[nodiscard]] auto frame_view () const -> const view_state &;

  //! Replaces the visible draw list used by scene passes.
  void set_visible_draws (std::vector<draw_command> draws);
  //! Returns the visible draw list for the active frame.
  [[nodiscard]] auto visible_draws () const
      -> const std::vector<draw_command> &;
  //! Clears the visible draw list for the current frame.
  void clear_visible_draws ();

  //! Sets the environment cubemap used by skybox and IBL rendering.
  void set_environment (const gfx::cubemap *env);

  //! Binds the default lit main-scene pipeline.
  void bind_main_pipeline ();
  //! Draws the current visible models.
  void draw_visible_models ();
  //! Draws outlines for submissions that requested highlighting.
  void draw_visible_model_outlines ();
  //! Builds SSAO from the current visible draw list.
  void build_ssao_for_visible_models ();
  //! Draws the active environment cubemap as the current skybox.
  void draw_active_environment ();

  /*!
   * \brief Draws a procedural infinite grid at the origin.
   * \param camera_pos The world-space position of the camera.
   * \param fog_center The world-space center of the visibility radius.
   * \param fog_radius The world-space radius of the Fog of War.
   */
  void draw_grid (const glm::vec3 &camera_pos, const glm::vec3 &fog_center,
                  float fog_radius);

  // Immediate debug line drawing (world-space lines rendered with depth/write)
  struct debug_vertex
  {
    glm::vec3 pos;
    glm::vec4 color;
  };
  void draw_debug_lines (const std::vector<debug_vertex> &verts,
                         const glm::mat4 &vp);

  //! Uploads the frame lighting buffer.
  void upload_lighting (const lighting_ubo &lighting);

  /*!
   * \brief Draws a model immediately using explicit transform data.
   */
  void draw_model (gfx::model_3d &model, size_t scene_index,
                   const glm::mat4 &model_matrix, const glm::mat4 &view_proj);

  //! Overrides the cached camera position used by shading.
  void set_camera_position (const glm::vec3 &position);
  //! Returns the camera position used by shading.
  [[nodiscard]] auto camera_position () const -> const glm::vec3 &;

  //! Forces subsequent model draws to use the unlit material path.
  void set_force_unlit (bool enabled);
  //! Returns true when the unlit override is active.
  [[nodiscard]] auto is_force_unlit () const -> bool;

  //! Sets the world-space direction used by the directional shadow cascade.
  void set_shadow_direction (const glm::vec3 &direction);
  //! Returns the world-space directional shadow direction.
  [[nodiscard]] auto shadow_direction () const -> const glm::vec3 &;

  //! Enables or disables directional shadow rendering.
  void set_directional_shadows_enabled (bool enabled);
  //! Returns true when directional shadows are enabled.
  [[nodiscard]] auto directional_shadows_enabled () const -> bool;
  //! Rebuilds the directional shadow matrix from the active camera state.
  void update_directional_shadow_view ();
  //! Returns the active directional light view-projection matrix.
  [[nodiscard]] auto directional_shadow_view () const -> const glm::mat4 &;

  //! Returns the default shadow depth bias.
  [[nodiscard]] auto shadow_map_bias () const -> float;
  //! Updates the default shadow depth bias.
  void set_shadow_map_bias (float bias);
  //! Returns the default shadow strength multiplier.
  [[nodiscard]] auto shadow_map_strength () const -> float;
  //! Updates the default shadow strength multiplier.
  void set_shadow_map_strength (float strength);
  //! Returns the shadow map resolution used by renderer-owned targets.
  [[nodiscard]] auto shadow_map_resolution () const -> uint32_t;

  //! Updates the intensity used by image-based lighting.
  void set_ibl_intensity (float intensity);

  //! Returns the renderer-owned spot shadow slots.
  auto spot_shadow_maps () -> std::array<shadow_map_2d, 4> &;
  //! Returns the renderer-owned spot shadow slots.
  [[nodiscard]] auto spot_shadow_maps () const
      -> const std::array<shadow_map_2d, 4> &;

  //! Returns the renderer-owned point shadow slots.
  auto point_shadow_maps () -> std::array<point_shadow_map, 2> &;
  //! Returns the renderer-owned point shadow slots.
  [[nodiscard]] auto point_shadow_maps () const
      -> const std::array<point_shadow_map, 2> &;

  /*!
   * \brief Builds a directional light matrix derived from the camera frustum.
   */
  static auto make_light_vp_from_camera (const glm::mat4 &cam_view,
                                         const glm::mat4 &cam_proj,
                                         const glm::vec3 &light_dir_world,
                                         float shadow_near = 0.1F,
                                         float shadow_far = 200.0F,
                                         float z_padding = 10.0F) -> glm::mat4;

  /*!
   * \brief Builds a spotlight view-projection matrix for a single light.
   */
  static auto make_spot_light_vp (const glm::vec3 &light_pos,
                                  const glm::vec3 &light_dir,
                                  float outer_angle_radians, float near_plane,
                                  float far_plane) -> glm::mat4;

  //! Begins the directional shadow pass.
  void begin_shadow_pass ();
  //! Ends the directional shadow pass.
  void end_shadow_pass ();
  //! Draws a model into the directional shadow map.
  void draw_model_shadow (gfx::model_3d &model, size_t scene_index,
                          const glm::mat4 &model_matrix);

  //! Begins a spotlight shadow pass for a renderer-owned slot.
  void begin_spot_shadow_pass (int index);
  //! Ends the active spotlight shadow pass.
  void end_spot_shadow_pass ();
  //! Draws a model into the active spotlight shadow pass.
  void draw_model_spot_shadow (gfx::model_3d &model, size_t scene_index,
                               const glm::mat4 &model_matrix,
                               const glm::mat4 &light_vp);

  //! Begins a point-light cubemap shadow face pass.
  void begin_point_shadow_pass (int index, int face);
  //! Ends the active point-light cubemap shadow face pass.
  void end_point_shadow_pass ();
  //! Draws a model into a point-light shadow cubemap face.
  void draw_model_point_shadow (gfx::model_3d &model, size_t scene_index,
                                const glm::mat4 &model_matrix,
                                const glm::mat4 &light_vp_mat,
                                const glm::vec3 &light_pos, float far_plane);

  /*!
   * \brief Returns the view-projection matrix for a point-light cubemap face.
   */
  static auto make_point_light_view_proj (const glm::vec3 &light_pos, int face,
                                          float near_plane, float far_plane)
      -> glm::mat4;

  //! Bakes irradiance, prefilter, and BRDF resources for a cubemap.
  void bake_ibl (gfx::cubemap &env);
  //! Bakes an equirectangular 2D texture into a cubemap.
  void bake_equirect_to_cube (gfx::cubemap &env, SDL_GPUTexture *equi_tex);
  //! Bakes the procedural skybox into a cubemap.
  void bake_procedural_skybox (gfx::cubemap &env, const glm::vec3 &sun_dir);
  //! Binds the preview background pipeline used by asset previews.
  void bind_preview_bg_pipeline (SDL_GPURenderPass *pass);

  //! Default clear color used by renderer-owned full-screen passes.
  SDL_FColor clear_color{ .r = 0.1F, .g = 0.15F, .b = 0.2F, .a = 1.0F };
  //! Enables or disables SSAO generation.
  bool ssao_enabled = true;
  //! Sampling radius in view space for SSAO.
  float ssao_radius = 0.6F;
  //! Bias applied to SSAO depth comparisons.
  float ssao_bias = 0.025F;
  //! Exponent applied to SSAO output.
  float ssao_power = 1.25F;
  //! Final multiplier applied to SSAO contribution.
  float ssao_intensity = 1.0F;
  //! Brightness threshold where bloom starts contributing.
  float bloom_threshold = 1.0F;
  //! Soft transition width around the bloom threshold.
  float bloom_knee = 0.5F;
  //! Final bloom contribution multiplier.
  float bloom_intensity = 1.0F;
  //! Color used by the outline pass.
  glm::vec4 outline_color{ 1.0F, 0.65F, 0.1F, 1.0F };
  //! Expansion distance used by the outline pass.
  float outline_width = 0.035F;

private:
  // High-level scene drawing helpers.
  [[nodiscard]] auto create_1x1_texture (uint8_t red, uint8_t green,
                                         uint8_t blue, uint8_t alpha) const
      -> SDL_GPUTexture *;
  [[nodiscard]] auto create_1x1_cubemap (uint8_t red, uint8_t green,
                                         uint8_t blue, uint8_t alpha) const
      -> SDL_GPUTexture *;
  static auto extract_position (const glm::mat4 &matrix) -> glm::vec3;
  auto select_lod (gfx::node &n) const -> gfx::mesh *;
  void bind_pipeline ();
  void render_mesh (const glm::mat4 &model, const glm::mat4 &view_proj,
                    const mesh &mesh);
  inline void render_node (gfx::node &n, const glm::mat4 &view_proj);
  void render_scene (gfx::scene &scene, const glm::mat4 &view_proj);
  void update_node_world (gfx::node &n, const glm::mat4 &parent);

  // Main scene resource management.
  void create_depth_texture (uint32_t width, uint32_t height);
  void create_pipeline ();
  void destroy_pipeline ();
  void create_unlit_pipeline ();
  void destroy_unlit_pipeline ();
  void create_default_texture ();
  void destroy_default_resources ();

  // Environment and preview helpers.
  void create_skybox_pipeline ();
  void bind_skybox_pipeline ();
  void draw_skybox (const gfx::cubemap &cubemap, const glm::mat4 &view,
                    const glm::mat4 &proj);
  void create_preview_bg_pipeline ();
  void destroy_preview_bg_pipeline ();
  void create_ibl_pipelines ();
  void destroy_ibl_pipelines ();

  // Shadow resource management and internal helpers.
  void create_shadow_resources (uint32_t size = 2048);
  void destroy_shadow_resources ();
  void render_shadow_map (gfx::scene &scene);
  void create_shadow_pipeline ();
  void create_point_shadow_pipeline ();
  void draw_model_shadow (gfx::model_3d &model, size_t scene_index,
                          const glm::mat4 &model_matrix,
                          SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd);

  // SSAO resource management and internal helpers.
  void create_ssao_resources (uint32_t width, uint32_t height);
  void destroy_ssao_resources ();
  void begin_ssao_prepass (const glm::mat4 &view, const glm::mat4 &proj);
  void end_ssao_prepass ();
  void draw_model_ssao (gfx::model_3d &model, size_t scene_index,
                        const glm::mat4 &model_matrix, const glm::mat4 &view,
                        const glm::mat4 &proj);
  void run_ssao_pass (const glm::mat4 &proj);
  void run_ssao_blur_pass ();
  void create_ssao_pipeline ();
  void destroy_ssao_pipeline ();
  void create_ssao_noise_texture (SDL_GPUCommandBuffer *cmd = nullptr);
  void create_ssao_kernel ();

  // Outline resource management and internal helpers.
  void create_outline_pipeline ();
  void destroy_outline_pipeline ();
  void draw_model_outline (gfx::model_3d &model, size_t scene_index,
                           const glm::mat4 &model_matrix,
                           const glm::mat4 &view_proj);

  // Grid helpers.
  void create_grid_pipeline ();
  void destroy_grid_pipeline ();

  // External dependencies and transient per-frame state.
  wsl::gfx::render_window *m_window = nullptr;
  render_context *m_ctx = nullptr;
  wsl::rsc::resource_manager *m_res_mgr = nullptr;
  view_state m_active_view{};
  std::vector<draw_command> m_visible_draws;
  const gfx::cubemap *m_active_env = nullptr;
  glm::vec3 m_camera_pos{ 0.0F, 0.0F, 0.0F };
  bool m_force_unlit = false;

  // CPU-side scratch storage used while building and traversing scenes.
  std::deque<mesh> m_meshes;
  std::vector<vertex> m_vertices;
  std::vector<uint32_t> m_indices;
  SDL_GPUTransferBuffer *m_vertex_transfer_buffer = nullptr;
  SDL_GPUTransferBuffer *m_index_transfer_buffer = nullptr;

  // Main scene pipelines.
  SDL_GPUGraphicsPipeline *m_pipeline = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_double_sided = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_unlit = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_unlit_double_sided = nullptr;

  // Shared material fallback resources.
  SDL_GPUTexture *m_default_texture = nullptr;
  SDL_GPUTexture *m_default_basecolor_tex = nullptr;
  SDL_GPUTexture *m_default_mr_tex = nullptr;
  SDL_GPUTexture *m_default_normal_tex = nullptr;
  SDL_GPUTexture *m_default_emissive_tex = nullptr;
  SDL_GPUTexture *m_default_cubemap_tex = nullptr;
  SDL_GPUTexture *m_default_brdf_lut_tex = nullptr;
  SDL_GPUSampler *m_default_sampler = nullptr;

  // Environment / skybox / IBL resources.
  SDL_GPUGraphicsPipeline *m_skybox_pipeline = nullptr;
  SDL_GPUTexture *m_skybox_cubemap = nullptr;
  SDL_GPUSampler *m_skybox_sampler = nullptr;
  SDL_GPUBuffer *m_skybox_vbo = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_preview_bg = nullptr;
  SDL_GPUGraphicsPipeline *m_ibl_irradiance_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_ibl_prefilter_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_ibl_brdf_lut_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_equi_to_cube_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_procedural_skybox_pipe = nullptr;
  SDL_GPUSampler *m_ibl_sampler = nullptr;
  glm::vec3 m_last_baked_sun_dir{ 0, 0, 0 };
  float m_ibl_intensity = 1.0F;
  float m_prefilter_max_mip = 0.0F;

  // Shadow state and resources.
  static constexpr int max_shadowed_spots = 4;
  static constexpr int max_shadowed_points = 2;
  glm::vec3 m_shadow_dir = glm::vec3 (0, -1, 0);
  bool m_shadows_enabled = true;
  glm::mat4 m_light_vp = glm::mat4 (1.0F);
  float m_shadow_bias = 0.0025F;
  float m_shadow_strength = 1.0F;
  uint32_t m_shadow_size = 2048;
  std::array<shadow_map_2d, 4> m_spot_shadows{};
  std::array<point_shadow_map, 2> m_point_shadows{};
  shadow_map_2d m_dir_shadow{};
  SDL_GPUGraphicsPipeline *m_shadow_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_shadow_pipe_double_sided = nullptr;
  SDL_GPUGraphicsPipeline *m_point_shadow_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_point_shadow_pipe_double_sided = nullptr;
  SDL_GPUTexture *m_shadow_depth = nullptr;
  SDL_GPUSampler *m_shadow_sampler = nullptr;
  SDL_GPUCommandBuffer *m_shadow_cmd = nullptr;
  SDL_GPURenderPass *m_shadow_pass = nullptr;

  // SSAO state and resources.
  SDL_GPUGraphicsPipeline *m_ssao_prepass_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_ssao_prepass_pipe_double_sided = nullptr;
  SDL_GPUGraphicsPipeline *m_ssao_pipe = nullptr;
  SDL_GPUGraphicsPipeline *m_ssao_blur_pipe = nullptr;
  SDL_GPUTexture *m_ssao_normal_depth = nullptr;
  SDL_GPUTexture *m_ssao_depth = nullptr;
  SDL_GPUTexture *m_ssao_raw = nullptr;
  SDL_GPUTexture *m_ssao_blur = nullptr;
  SDL_GPUTexture *m_ssao_noise_tex = nullptr;
  SDL_GPUSampler *m_ssao_linear_sampler = nullptr;
  SDL_GPUSampler *m_ssao_point_sampler = nullptr;
  SDL_GPURenderPass *m_ssao_prepass = nullptr;
  uint32_t m_ssao_width = 0;
  uint32_t m_ssao_height = 0;
  std::array<glm::vec4, 64> m_ssao_kernel{};

  // Outline resources.
  SDL_GPUGraphicsPipeline *m_pipeline_outline = nullptr;
  SDL_GPUGraphicsPipeline *m_pipeline_outline_double_sided = nullptr;

  // Debug line pipeline (for editor gizmos)
  SDL_GPUGraphicsPipeline *m_pipeline_debug_lines = nullptr;

  // Grid resources.
  //! Graphics pipeline for the procedural projected grid.
  SDL_GPUGraphicsPipeline *m_pipeline_grid = nullptr;
};

} // namespace gfx

} // namespace wsl
