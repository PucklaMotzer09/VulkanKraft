#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace core {
class Window {
public:
  class Mouse {
  public:
    friend class Window;

    Mouse() : screen_position(0.0f, 0.0f) {}

    glm::vec2 screen_position;

    bool button_is_pressed(int button) const;
    bool button_just_pressed(int button) const;

  private:
    void _on_mouse_button(int button, int action, int mods);

    std::map<int, bool> m_pressed_buttons;
    std::map<int, bool> m_previous_pressed_buttons;
  };

  static void on_framebuffer_resize(GLFWwindow *window, int width, int height);

  Window(const uint32_t width, const uint32_t height, std::string title);
  ~Window();

  inline bool should_close() const { return glfwWindowShouldClose(m_window); }
  void poll_events();
  std::vector<const char *> get_required_vulkan_extensions() const;
  vk::SurfaceKHR create_vulkan_surface(vk::Instance &instance) const;
  inline std::pair<uint32_t, uint32_t> get_framebuffer_size() const {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    return std::make_pair(static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height));
  }
  inline void
  set_on_resize(std::function<void(uint32_t width, uint32_t height)> callback) {
    m_fb_resize_callbacks.emplace_back(callback);
  }
  inline const Mouse &get_mouse() const { return m_mouse; }

  void reset_keys();
  bool key_is_pressed(int key) const;
  bool key_just_pressed(int key) const;
  void lock_cursor();
  void release_cursor();
  bool cursor_is_locked();

private:
  static void _on_key_callback(GLFWwindow *window, int key, int scancode,
                               int action, int mods);
  static void _on_cursor_pos_callback(GLFWwindow *window, double x, double y);
  static void _on_mouse_button_callback(GLFWwindow *window, int button,
                                        int action, int mods);

  void _on_key(int key, int scancode, int action, int mods);
  void _on_cursor_position(double x, double y);

  GLFWwindow *m_window;
  std::vector<std::function<void(uint32_t width, uint32_t height)>>
      m_fb_resize_callbacks;
  std::map<int, bool> m_pressed_keys;
  std::map<int, bool> m_previous_pressed_keys;
  Mouse m_mouse;
};
} // namespace core