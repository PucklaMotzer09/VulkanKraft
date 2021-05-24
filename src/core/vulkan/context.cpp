#include "context.hpp"
#include "../exception.hpp"
#include "../log.hpp"
#include <cstring>

namespace core {
namespace vulkan {
Context::Context(const Window &window) { _create_instance(window); }

Context::~Context() { m_instance.destroy(); }

bool Context::_has_validation_layer_support(
    const std::vector<const char *> &layer_names) noexcept {
  const auto layers(vk::enumerateInstanceLayerProperties());

  for (const auto *ln : layer_names) {
    bool found = false;
    for (const auto &l : layers) {
      if (strcmp(l.layerName.data(), ln) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }

  return true;
}

void Context::_populate_debug_messenger_create_info(
    vk::DebugUtilsMessengerCreateInfoEXT &di) noexcept {
  di.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  di.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
  di.pfnUserCallback = _debug_callback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  std::string msg("\033[32m[Validation Layers]\033[0m ");
  msg += pCallbackData->pMessage;

  switch (
      static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) {
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
    Log::error(msg);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
    Log::warning(msg);
    break;
  default:
    Log::info(msg);
    break;
  }

  return VK_FALSE;
}

void Context::_create_instance(const Window &window) {
  const std::vector<const char *> validation_layers = {
      "VK_LAYER_KHRONOS_validation"};

  if (_enable_validation_layers &&
      !_has_validation_layer_support(validation_layers)) {
    throw VulkanKraftException("validation layers are not supported");
  }

  vk::ApplicationInfo ai;
  ai.pApplicationName = _application_name;
  ai.applicationVersion = _application_version;
  ai.pEngineName = _engine_name;
  ai.engineVersion = _engine_version;
  ai.apiVersion = VK_API_VERSION_1_0;

  vk::InstanceCreateInfo ii;
  ii.pApplicationInfo = &ai;

  auto extensions = window.get_required_vulkan_extensions();
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  ii.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ii.ppEnabledExtensionNames = extensions.data();

  vk::DebugUtilsMessengerCreateInfoEXT di;
  if constexpr (_enable_validation_layers) {
    ii.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    ii.ppEnabledLayerNames = validation_layers.data();

    _populate_debug_messenger_create_info(di);
    ii.pNext = &di;
  }

  try {
    m_instance = vk::createInstance(ii);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(std::string("failed to create instance: ") +
                               e.what());
  }
}
} // namespace vulkan
} // namespace core