#include "ingame_scene.hpp"
#include "glm/gtx/transform.hpp"
#include "main_menu_scene.hpp"

InGameScene::InGameScene(const core::vulkan::Context &context,
                         core::ResourceHodler &hodler, core::Window &window,
                         const core::Settings &settings,
                         const glm::mat4 &projection, const bool new_world,
                         const size_t world_seed)
    : m_physics_server(1.0f / static_cast<float>(settings.max_fps)),
      m_fps_text(context,
                 hodler.get_font(core::ResourceHodler::debug_font_name),
                 L"60 FPS"),
      m_position_text(context,
                      hodler.get_font(core::ResourceHodler::debug_font_name),
                      L"X\nY\nZ\n"),
      m_look_text(context,
                  hodler.get_font(core::ResourceHodler::debug_font_name),
                  L"Look\nX\nY\nZ"),
      m_vel_text(context,
                 hodler.get_font(core::ResourceHodler::debug_font_name),
                 L"Velocity"),
      m_player(glm::vec3(128.0f, 70.0f, 128.0f), hodler, m_physics_server),
      m_world(context, m_block_server),
      m_chunk_shader(
          hodler.get_shader(core::ResourceHodler::chunk_mesh_shader_name)),
      m_line_3d_shader(
          hodler.get_shader(core::ResourceHodler::line_3d_shader_name)),
      m_context(context), m_settings(settings), m_hodler(hodler),
      m_window(window), m_projection(projection) {

  const std::filesystem::path world_save_folder(
      settings.settings_folder / core::Settings::world_save_folder_name /
      "test_world");

  if (new_world) {
    std::filesystem::remove_all(world_save_folder);
  }

  m_world.set_save_folder(world_save_folder, world_seed);

  m_world.set_center_position(m_player.position);
  m_fog_max_distance = m_world.set_render_distance(settings.render_distance);
  m_world.start_update_thread();

  // Position texts
  {
    glm::vec2 current_pos(0.0f, m_fps_text.get_height() + 10);
    m_position_text.set_position(current_pos);
    current_pos.y += m_position_text.get_height() + 10;
    m_look_text.set_position(current_pos);
    current_pos.y += m_look_text.get_height() + 10;
    m_vel_text.set_position(current_pos);
  }

  // Wait until some chunks have been generated
  m_world.wait_for_generation(settings.render_distance * 2 *
                              settings.render_distance * 2);
  // Set Player Data
  if (const auto player_data(m_world.get_save_world()->read_player_data());
      player_data) {
    m_player.position = player_data->position;
    m_player.velocity = player_data->velocity;
    m_player.set_rotation(player_data->rotation);
  } else if (const auto player_height(m_world.get_height(m_player.position));
             player_height) {
    // Place the player at the correct height
    m_player.position.y = static_cast<float>(*player_height);
  }

  // Initialise selected block
  m_selected_block.begin();

  // FRONT
  m_selected_block.add_vertex(glm::vec3(0.0f, 0.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(0.0f, 1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 0.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(0.0f, 0.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));

  // LEFT
  m_selected_block.add_vertex(glm::vec3(0.0f, 0.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(0.0f, 1.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(0.0f, 1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));

  // TOP
  m_selected_block.add_vertex(glm::vec3(1.0f, 1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 1.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(0.0f, 1.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));

  // BACK
  m_selected_block.add_vertex(glm::vec3(0.0f, 0.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 0.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 1.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));

  // RIGHT
  m_selected_block.add_vertex(glm::vec3(1.0f, 0.0f, 1.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));
  m_selected_block.add_vertex(glm::vec3(1.0f, 0.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 1.0f));

  m_selected_block.end(context);
}

InGameScene::~InGameScene() {
  // Save player data
  const save::World::PlayerData player_data{
      m_player.position, m_player.velocity, m_player.get_rotation()};
  m_world.get_save_world()->write_player_data(player_data);
}

std::unique_ptr<scene::Scene> InGameScene::update(core::Window &window,
                                                  const float delta_time) {
  if (window.key_just_pressed(back_keyboard_button) ||
      window.gamepad_button_just_pressed(back_gamepad_button)) {
    if (!window.cursor_is_locked())
      return std::make_unique<MainMenuScene>(m_context, m_hodler, m_settings,
                                             m_window, m_projection);
    window.release_cursor();
  }

  // Generate the texts for every text element
  {
    std::wstringstream fps_stream;
    fps_stream << std::setprecision(0) << std::fixed << (1.0f / delta_time);
    fps_stream << L" FPS";
    m_fps_text.set_string(fps_stream.str());
  }
  {
    constexpr auto float_width = 8;
    std::wstringstream pos_stream;
    pos_stream << std::fixed << std::setprecision(1);
    pos_stream << "X:" << std::setw(float_width) << std::right
               << m_player.position.x << std::endl;
    pos_stream << "Y:" << std::setw(float_width) << std::right
               << m_player.position.y << std::endl;
    pos_stream << "Z:" << std::setw(float_width) << std::right
               << m_player.position.z << std::endl;
    m_position_text.set_string(pos_stream.str());
  }
  {
    const auto look_dir(m_player.get_look_direction());
    constexpr auto float_width = 8;
    std::wstringstream stream;
    stream << "Look" << std::endl;
    stream << std::fixed << std::setprecision(3);
    stream << "X:" << std::setw(float_width) << std::right << look_dir.x
           << std::endl;
    stream << "Y:" << std::setw(float_width) << std::right << look_dir.y
           << std::endl;
    stream << "Z:" << std::setw(float_width) << std::right << look_dir.z
           << std::endl;
    m_look_text.set_string(stream.str());
  }
  {
    constexpr auto float_width = 8;
    std::wstringstream stream;
    stream << "Velocity" << std::endl;
    stream << std::fixed << std::setprecision(3);
    stream << "X:" << std::setw(float_width) << std::right
           << m_player.velocity.x << std::endl;
    stream << "Y:" << std::setw(float_width) << std::right
           << m_player.velocity.y << std::endl;
    stream << "Z:" << std::setw(float_width) << std::right
           << m_player.velocity.z << std::endl;
    m_vel_text.set_string(stream.str());
  }

  // Some debug input for testing purposes
  if (window.key_just_pressed(reseed_keyboard_button)) {
    m_world.clear_and_reseed();
  } else if (window.key_just_pressed(place_player_keyboard_button)) {
    // Place player at the correct height at the current position
    if (const auto player_height(m_world.get_height(m_player.position));
        player_height) {
      m_player.position.y = static_cast<float>(*player_height);
    }
  }

  m_player.update(window, m_world);
  {
    const auto block_pos(m_player.get_selected_block_position(m_world));
    if (block_pos) {
      m_selected_position = glm::vec3(block_pos->x, block_pos->y, block_pos->z);
    } else {
      m_selected_position.reset();
    }
  }
  m_world.set_center_position(m_player.position);

  m_physics_server.update(m_world, delta_time);

  // Update the uniforms
  // NOTE: This is the projection matrix of the last frame. But this shouldn't
  // be an issue
  m_chunk_global.proj_view = m_projection * m_player.create_view_matrix();
  m_chunk_global.eye_pos = m_player.get_eye_position();

  return nullptr;
}

void InGameScene::render(const core::vulkan::RenderCall &render_call,
                         const float delta_time) {
  m_chunk_shader.update_uniform_buffer(render_call, m_chunk_global);
  if (m_selected_position) {
    m_line_3d_shader.update_uniform_buffer(render_call,
                                           m_chunk_global.proj_view);
  }

  // Render the world
  m_chunk_shader.bind(render_call);
  // fog max distance
  m_chunk_shader.set_push_constant(render_call, m_fog_max_distance);
  m_world.render(render_call);

  // Render selected block
  if (m_selected_position) {
    core::Line3D::bind_shader(render_call);
    m_selected_block.set_model_matrix(*m_selected_position);
    m_selected_block.render(render_call);
  }

  // Render the player
  core::Render2D::bind_shader(render_call);
  m_player.render(render_call);

  // Render the text elements
  core::text::Text::bind_shader(render_call);
  m_fps_text.render(render_call);
  m_position_text.render(render_call);
  m_look_text.render(render_call);
  m_vel_text.render(render_call);
}