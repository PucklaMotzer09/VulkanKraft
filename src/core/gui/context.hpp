#pragma once
#include "../shader.hpp"
#include "../text/font.hpp"
#include "../vulkan/context.hpp"
#include "../window.hpp"
#include "element.hpp"
#include <memory>
#include <vector>

namespace core {
namespace gui {
class Context {
public:
  friend class Element;
  friend class Button;

  Context(const vulkan::Context &vulkan_context, Window &window,
          text::Font &text_font);

  template <typename T, typename... A> inline auto *add_element(A... args) {
    auto *element = new T(args...);
    m_elements.emplace_back(element);
    return element;
  }
  template <typename T> inline auto *emplace_element(T *element) {
    m_elements.emplace_back(element);
    return element;
  }

  void update();
  void render(const vulkan::RenderCall &render_call);

private:
  std::vector<std::unique_ptr<Element>> m_elements;

  const vulkan::Context &m_vulkan_context;
  Window &m_window;
  text::Font &m_text_font;
};
} // namespace gui
} // namespace core
