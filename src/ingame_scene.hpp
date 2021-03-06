#pragma once

#include "block/server.hpp"
#include "chunk/world.hpp"
#include "core/line_3d.hpp"
#include "core/resource_hodler.hpp"
#include "core/settings.hpp"
#include "core/text/text.hpp"
#include "physics/server.hpp"
#include "player.hpp"
#include "scene/scene.hpp"
#include <optional>

class InGameScene : public scene::Scene {
public:
  InGameScene(const core::vulkan::Context &context,
              core::ResourceHodler &resource_hodler, core::Window &window,
              const core::Settings &settings, const glm::mat4 &projection,
              const bool new_world = false, const size_t world_seed = 0);
  ~InGameScene();

  std::unique_ptr<scene::Scene> update(core::Window &window,
                                       const float delta_time) override;
  void render(const core::vulkan::RenderCall &render_call,
              const float delta_time) override;

private:
  static constexpr int reseed_keyboard_button = GLFW_KEY_F8;
  static constexpr int place_player_keyboard_button = GLFW_KEY_F9;
  static constexpr int back_keyboard_button = GLFW_KEY_ESCAPE;
  static constexpr int back_gamepad_button = GLFW_GAMEPAD_BUTTON_START;

  block::Server m_block_server;
  physics::Server m_physics_server;

  core::text::Text m_fps_text;
  core::text::Text m_position_text;
  core::text::Text m_look_text;
  core::text::Text m_vel_text;

  Player m_player;
  chunk::World m_world;
  core::Line3D m_selected_block;
  std::optional<glm::vec3> m_selected_position;

  core::Shader &m_chunk_shader;
  core::Shader &m_line_3d_shader;

  float m_fog_max_distance;
  chunk::GlobalUniform m_chunk_global;
  const core::vulkan::Context &m_context;
  const core::Settings &m_settings;
  core::ResourceHodler &m_hodler;
  core::Window &m_window;
  const glm::mat4 &m_projection;
};