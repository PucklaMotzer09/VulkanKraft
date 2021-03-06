#pragma once
#include "shader.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/render_call.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace core {
// Renders a custom amount of 3D lines with vertex colors using LineStrip
// primitive topology
class Line3D {
public:
  class Vertex {
  public:
    Vertex(const glm::vec3 &pos, const glm::vec3 &col)
        : position(pos), color(col) {}

    glm::vec3 position;
    glm::vec3 color;
  };

  static inline void set_shader(Shader &shader) { m_shader = &shader; }
  static inline void bind_shader(const vulkan::RenderCall &render_call) {
    m_shader->bind(render_call);
  }

  void begin();
  void end(const vulkan::Context &context);
  inline void add_vertex(const glm::vec3 &pos, const glm::vec3 &color) {
    m_current_vertices.emplace_back(pos, color);
  }

  void render(const vulkan::RenderCall &render_call);
  void set_model_matrix(const glm::vec3 &position);

private:
  static Shader *m_shader;

  std::unique_ptr<vulkan::Buffer> m_vertex_buffer;
  std::vector<Vertex> m_current_vertices;
  uint32_t m_num_vertices;
  glm::mat4 m_model_matrix;
};
} // namespace core
