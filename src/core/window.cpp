#include "window.hpp"
#include "exception.hpp"
#include "log.hpp"
#include <fstream>

namespace core {
bool Window::Mouse::button_is_pressed(int button) const {
  return m_pressed_buttons.find(button) != m_pressed_buttons.end() &&
         m_pressed_buttons.at(button);
}

bool Window::Mouse::button_just_pressed(int button) const {
  return button_is_pressed(button) &&
         (m_previous_pressed_buttons.find(button) ==
              m_previous_pressed_buttons.end() ||
          !m_previous_pressed_buttons.at(button));
}

bool Window::Mouse::button_was_pressed(int button) const {
  return m_previous_pressed_buttons.find(button) !=
             m_previous_pressed_buttons.end() &&
         m_previous_pressed_buttons.at(button);
}

void Window::Mouse::_on_mouse_button(int button, int action, int mods) {
  m_pressed_buttons[button] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

void Window::on_framebuffer_resize(GLFWwindow *window, int width, int height) {
  auto *win = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  for (auto &cb : win->m_fb_resize_callbacks) {
    cb(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  }
}

void Window::on_joystick_event(int jid, int event) {
  std::string name("No Name");
  if (const char *_name = glfwGetJoystickName(jid); _name) {
    name = _name;
  }

  if (event == GLFW_CONNECTED) {
    Log::info("Joystick \"" + name + "\" just connected");
  } else if (event == GLFW_DISCONNECTED) {
    Log::info("Joystick \"" + name + "\" just disconnected");
  }
}

Window::Window(const uint32_t width, const uint32_t height, std::string title)
    : m_is_fullscreen(false), m_joystick1(-1) {
  if (glfwInit() == GLFW_FALSE) {
    throw VulkanKraftException("failed to initialise GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  if (m_window =
          glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
      !m_window) {
    throw VulkanKraftException("failed to create window");
  }

  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, on_framebuffer_resize);
  glfwSetKeyCallback(m_window, _on_key_callback);
  glfwSetCursorPosCallback(m_window, _on_cursor_pos_callback);
  glfwSetMouseButtonCallback(m_window, _on_mouse_button_callback);

  glfwSetJoystickCallback(on_joystick_event);
}

Window::~Window() {
  glfwDestroyWindow(m_window);
  glfwTerminate();
}

void Window::poll_events() {
  glfwPollEvents();

  if (m_joystick1 != -1) {
    GLFWgamepadstate gamepad_state;
    if (glfwGetGamepadState(m_joystick1, &gamepad_state)) {
      for (int b = min_gamepad_button; b <= max_gamepad_button; b++) {
        m_pressed_gamepad_buttons[b] = gamepad_state.buttons[b];
      }
      for (int a = min_gamepad_axis; a <= max_gamepad_axis; a++) {
        m_gamepad_axes[a] = gamepad_state.axes[a];
      }
    } else {
      int count;
      const auto *buttons = glfwGetJoystickButtons(m_joystick1, &count);
      if (buttons) {
        for (int b = 0; b < count; b++) {
          m_pressed_gamepad_buttons[b] = buttons[b] == GLFW_PRESS;
        }
      }

      const auto *axes = glfwGetJoystickAxes(m_joystick1, &count);
      if (axes) {
        for (int a = 0; a < count; a++) {
          m_gamepad_axes[a] = axes[a];
        }
      }
    }
  }
}

std::vector<const char *> Window::get_required_vulkan_extensions() const {
  uint32_t glfw_count;
  const auto **glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_count);
  if (glfw_count == 0 || !glfw_ext) {
    throw VulkanKraftException(
        "failed to get required vulkan extensions from GLFW");
  }

  return std::vector(glfw_ext, glfw_ext + glfw_count);
}

vk::SurfaceKHR Window::create_vulkan_surface(vk::Instance &instance) const {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(instance.operator VkInstance(), m_window, nullptr,
                              &surface) != VK_SUCCESS) {
    throw VulkanKraftException("failed to create surface");
  }

  return surface;
}

void Window::reset_keys() {
  for (auto [k, p] : m_pressed_keys) {
    m_previous_pressed_keys[k] = p;
  }
  for (auto [k, b] : m_mouse.m_pressed_buttons) {
    m_mouse.m_previous_pressed_buttons[k] = b;
  }
  for (auto [k, b] : m_pressed_gamepad_buttons) {
    m_previous_pressed_gamepad_buttons[k] = b;
  }
}

bool Window::key_is_pressed(int key) const {
  return m_pressed_keys.find(key) != m_pressed_keys.end() &&
         m_pressed_keys.at(key);
}

bool Window::key_just_pressed(int key) const {
  return key_is_pressed(key) &&
         (m_previous_pressed_keys.find(key) == m_previous_pressed_keys.end() ||
          !m_previous_pressed_keys.at(key));
}

void Window::lock_cursor() {
  glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::release_cursor() {
  glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool Window::cursor_is_locked() {
  return glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

void Window::toggle_fullscreen() {
  if (!m_is_fullscreen) {
    glfwGetFramebufferSize(m_window, &m_previous_width, &m_previous_height);
    glfwGetWindowPos(m_window, &m_previous_x, &m_previous_y);

    int monitor_count;
    auto **monitors = glfwGetMonitors(&monitor_count);
    if (monitor_count == 0) {
      Log::warning("no monitors for fullscreen have been detected");
      return;
    }

    int x_pos, y_pos, width, height;
    glfwGetMonitorWorkarea(monitors[0], &x_pos, &y_pos, &width, &height);

    glfwSetWindowMonitor(m_window, monitors[0], 0, 0, 0, 0, GLFW_DONT_CARE);
    glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowSize(m_window, width, height);
    glfwSetWindowPos(m_window, x_pos, y_pos);
    m_is_fullscreen = true;
  } else {
    glfwSetWindowMonitor(m_window, nullptr, m_previous_x, m_previous_y,
                         m_previous_width, m_previous_height, GLFW_DONT_CARE);
    glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowSize(m_window, m_previous_width, m_previous_height);
    glfwSetWindowPos(m_window, m_previous_x, m_previous_y);
    m_is_fullscreen = false;
  }
}

bool Window::is_gamepad_connected() {
  if (m_joystick1 == -1 || !glfwJoystickIsGamepad(m_joystick1)) {
    for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
      if (glfwJoystickIsGamepad(i)) {
        m_joystick1 = i;
        Log::info("Using \"" + std::string(glfwGetJoystickName(m_joystick1)) +
                  "\" as Gamepad");
        return true;
      }
    }

    return false;
  }

  return true;
}

bool Window::gamepad_button_is_pressed(int button) const {
  return m_pressed_gamepad_buttons.find(button) !=
             m_pressed_gamepad_buttons.end() &&
         m_pressed_gamepad_buttons.at(button);
}

bool Window::gamepad_button_just_pressed(int button) const {
  return gamepad_button_is_pressed(button) &&
         (m_pressed_gamepad_buttons.find(button) ==
              m_pressed_gamepad_buttons.end() ||
          !m_previous_pressed_gamepad_buttons.at(button));
}

std::optional<float> Window::get_gamepad_axis_value(int axis) const {
  if (m_gamepad_axes.find(axis) == m_gamepad_axes.end()) {
    return std::nullopt;
  }

  return m_gamepad_axes.at(axis);
}

void Window::update_controller_db(
    const std::filesystem::path &file_name) const {
  std::ifstream file;
  file.open(file_name, std::ios_base::ate);
  if (file.fail()) {
    Log::warning("failed open controller db file for reading");
    return;
  }

  const auto file_size{file.tellg()};
  file.seekg(0);
  std::string buffer;
  buffer.resize(file_size);
  file.read(buffer.data(), file_size);
  file.close();

  if (glfwUpdateGamepadMappings(buffer.c_str()) != GLFW_TRUE) {
    Log::warning("failed to update glfw gamepad mappings");
  } else {
    Log::info("Successfully updated glfw gamepad mappings");
  }
}

void Window::_on_key_callback(GLFWwindow *window, int key, int scancode,
                              int action, int mods) {
  auto *win = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  win->_on_key(key, scancode, action, mods);
}

void Window::_on_cursor_pos_callback(GLFWwindow *window, double x, double y) {
  auto *win = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  win->_on_cursor_position(x, y);
}

void Window::_on_mouse_button_callback(GLFWwindow *window, int button,
                                       int action, int mods) {
  auto *win = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  win->m_mouse._on_mouse_button(button, action, mods);
}

void Window::_on_key(int key, int scancode, int action, int mods) {
  m_pressed_keys[key] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

void Window::_on_cursor_position(double x, double y) {
  m_mouse.screen_position.x = static_cast<float>(x);
  m_mouse.screen_position.y = static_cast<float>(y);
}
} // namespace core