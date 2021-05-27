#include "core/exception.hpp"
#include "core/log.hpp"
#include "core/shader.hpp"
#include "core/texture.hpp"
#include "core/vulkan/buffer.hpp"
#include "core/vulkan/context.hpp"
#include "core/vulkan/vertex.hpp"
#include "core/window.hpp"
#include <chrono>
#include <fstream>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <thread>

using namespace std::literals::chrono_literals;

constexpr uint32_t window_width = 840;
constexpr uint32_t window_height = 480;
constexpr char window_title[] = "VulkanKraft";

struct Transform {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

int main(int args, char *argv[]) {
  const auto vertices =
      std::array{core::vulkan::Vertex(-0.5f, 0.5f, -1.0f, 0.0f, 1.0f),
                 core::vulkan::Vertex(0.5f, -0.5f, -1.0f, 1.0f, 0.0f),
                 core::vulkan::Vertex(0.5f, 0.5f, -1.0f, 1.0f, 1.0f),
                 core::vulkan::Vertex(-0.5f, -0.5f, -1.0f, 0.0f, 0.0f),
                 core::vulkan::Vertex(0.5f, -0.5f, -1.0f, 1.0f, 0.0f),
                 core::vulkan::Vertex(-0.5f, 0.5f, -1.0f, 0.0f, 1.0f)};

  try {
    core::Window window(window_width, window_height, window_title);
    core::vulkan::Context context(window);

    std::vector<uint8_t> texture_data(512 * 512 * 4, 255);
    auto texture = core::Texture::Builder().dimensions(512, 512).build(
        context, texture_data.data());
    texture_data.clear();

    Transform ubo0;
    ubo0.model = glm::identity<glm::mat4>();
    ubo0.view = glm::identity<glm::mat4>();
    ubo0.proj = glm::identity<glm::mat4>();

    auto shader = core::Shader::Builder()
                      .vertex_attribute<glm::vec3>()
                      .vertex_attribute<glm::vec2>()
                      .vertex("shaders_spv/triangle.vert.spv")
                      .fragment("shaders_spv/triangle.frag.spv")
                      .uniform_buffer(vk::ShaderStageFlagBits::eVertex, ubo0)
                      .build(context);

    core::vulkan::Buffer vertex_buffer(context,
                                       vk::BufferUsageFlagBits::eVertexBuffer,
                                       sizeof(vertices), vertices.data());

    while (!window.should_close()) {
      window.poll_events();

      const auto [width, height] = window.get_framebuffer_size();
      ubo0.proj = glm::perspective(
          glm::radians(75.0f),
          static_cast<float>(width) / static_cast<float>(height), 0.01f, 2.0f);
      ubo0.proj[1][1] *= -1.0f;
      ubo0.model =
          glm::rotate(glm::radians(90.0f * static_cast<float>(glfwGetTime())),
                      glm::vec3(0.0f, 0.0f, 1.0f));

      if (const auto render_call = context.render_begin(); render_call) {
        shader.update_uniform_buffer(render_call.value(), ubo0);
        shader.bind(render_call.value());
        vertex_buffer.bind(render_call.value());
        render_call.value().render_vertices(vertices.size());
      }

      context.render_end();

      std::this_thread::sleep_for(10ms);
    }
  } catch (const core::VulkanKraftException &e) {
    core::Log::error(e.what());
    return 1;
  }

  return 0;
}