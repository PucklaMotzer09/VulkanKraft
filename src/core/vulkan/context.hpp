#pragma once
#include "../window.hpp"
#include "swap_chain.hpp"
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace core {
namespace vulkan {
class Context {
public:
  friend class SwapChain;
  friend class GraphicsPipeline;

  static PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
  static PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

  Context(const Window &window);
  ~Context();

private:
  class QueueFamilyIndices {
  public:
    QueueFamilyIndices(const vk::PhysicalDevice &device,
                       const vk::SurfaceKHR &surface);

    bool is_complete() const;

    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
  };

  class SwapChainSupportDetails {
  public:
    SwapChainSupportDetails(const vk::PhysicalDevice &device,
                            const vk::SurfaceKHR &surface);

    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
  };

  static constexpr char _application_name[] = "VulkanKraft";
  static constexpr uint32_t _application_version =
      VK_MAKE_API_VERSION(0, 0, 0, 0);
  static constexpr char _engine_name[] = "VulkanKraft Core";
  static constexpr uint32_t _engine_version = VK_MAKE_API_VERSION(0, 0, 0, 0);
#ifdef NDEBUG
  static constexpr bool _enable_validation_layers = false;
#else
  static constexpr bool _enable_validation_layers = true;
#endif

  static bool _has_validation_layer_support(
      const std::vector<const char *> &layer_names) noexcept;
  static void _populate_debug_messenger_create_info(
      vk::DebugUtilsMessengerCreateInfoEXT &di) noexcept;
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  _debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                  void *pUserData);
  static bool
  _is_device_suitable(const vk::PhysicalDevice &device,
                      const vk::SurfaceKHR &surface,
                      const std::vector<const char *> &extension_names);
  static bool _device_has_extension_support(
      const vk::PhysicalDevice &device,
      const std::vector<const char *> &extension_names);

  // ****** Utilities ***********
  static vk::Format
  _find_supported_format(const vk::PhysicalDevice &device,
                         const std::vector<vk::Format> &candidates,
                         vk::ImageTiling tiling,
                         vk::FormatFeatureFlags features);
  static vk::Format _find_depth_format(const vk::PhysicalDevice &device);
  // ****************************

  // ****** Initialisation ******
  void _create_instance(const Window &window,
                        const std::vector<const char *> &validation_layers);
  void _setup_debug_messenger();
  void _create_surface(const Window &window);
  void _pick_physical_device(const std::vector<const char *> &extensions);
  void
  _create_logical_device(const std::vector<const char *> &extensions,
                         const std::vector<const char *> &validation_layers);
  void _create_swap_chain(const Window &window);
  void _create_render_pass();
  // ****************************

  vk::Instance m_instance;
  vk::DebugUtilsMessengerEXT m_debug_messenger;
  vk::SurfaceKHR m_surface;
  vk::PhysicalDevice m_physical_device;
  vk::Device m_device;
  vk::Queue m_graphics_queue;
  vk::Queue m_present_queue;
  std::unique_ptr<SwapChain> m_swap_chain;
  vk::RenderPass m_render_pass;
};
} // namespace vulkan
} // namespace core