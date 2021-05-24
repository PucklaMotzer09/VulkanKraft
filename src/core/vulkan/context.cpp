#include "context.hpp"
#include "../exception.hpp"
#include "../log.hpp"
#include <array>
#include <cstring>
#include <set>

PFN_vkCreateDebugUtilsMessengerEXT
    core::vulkan::Context::pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT
    core::vulkan::Context::pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger) {
  return core::vulkan::Context::pfnVkCreateDebugUtilsMessengerEXT(
      instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const *pAllocator) {
  return core::vulkan::Context::pfnVkDestroyDebugUtilsMessengerEXT(
      instance, messenger, pAllocator);
}

namespace core {
namespace vulkan {
Context::Context(const Window &window) {
  const std::vector<const char *> validation_layers = {
      "VK_LAYER_KHRONOS_validation"};
  _create_instance(window, validation_layers);
  _setup_debug_messenger();
  _create_surface(window);
  {
    const std::vector<const char *> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    _pick_physical_device(device_extensions);
    _create_logical_device(device_extensions, validation_layers);
  }
  _create_swap_chain(window);
  _create_render_pass();
  m_swap_chain->create_framebuffers();

  Log::info("Successfully Constructed Vulkan Context");
}

Context::~Context() {
  m_swap_chain.reset();
  m_device.destroyRenderPass(m_render_pass);
  m_device.destroy();
  if constexpr (_enable_validation_layers) {
    m_instance.destroyDebugUtilsMessengerEXT(m_debug_messenger);
  }
  m_instance.destroySurfaceKHR(m_surface);
  m_instance.destroy();
}

Context::QueueFamilyIndices::QueueFamilyIndices(
    const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface) {
  const auto queue_families(device.getQueueFamilyProperties());

  int i = 0;
  for (const auto &qf : queue_families) {
    if (qf.queueFlags & vk::QueueFlagBits::eGraphics) {
      graphics_family = i;
    }

    const auto present_support = device.getSurfaceSupportKHR(i, surface);
    if (present_support) {
      present_family = i;
    }

    if (is_complete()) {
      break;
    }

    i++;
  }
}

bool Context::QueueFamilyIndices::is_complete() const {
  return graphics_family.has_value() && present_family.has_value();
}

Context::SwapChainSupportDetails::SwapChainSupportDetails(
    const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface)
    : capabilities(device.getSurfaceCapabilitiesKHR(surface)),
      formats(device.getSurfaceFormatsKHR(surface)),
      present_modes(device.getSurfacePresentModesKHR(surface)) {}

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

bool Context::_is_device_suitable(
    const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface,
    const std::vector<const char *> &extension_names) {
  const QueueFamilyIndices indices(device, surface);

  const auto extensions_supported =
      _device_has_extension_support(device, extension_names);
  bool swap_chain_adequate = false;
  if (extensions_supported) {
    const SwapChainSupportDetails swap_chain_support(device, surface);
    swap_chain_adequate = !swap_chain_support.formats.empty() &&
                          !swap_chain_support.present_modes.empty();
  }

  const auto features = device.getFeatures();

  return indices.is_complete() && extensions_supported && swap_chain_adequate &&
         features.samplerAnisotropy;
}

bool Context::_device_has_extension_support(
    const vk::PhysicalDevice &device,
    const std::vector<const char *> &extension_names) {
  const auto extensions(device.enumerateDeviceExtensionProperties());

  for (const auto *en : extension_names) {
    bool found = false;
    for (const auto &e : extensions) {
      if (strcmp(e.extensionName.data(), en) == 0) {
        found = true;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

vk::Format Context::_find_supported_format(
    const vk::PhysicalDevice &device, const std::vector<vk::Format> &candidates,
    vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
  for (const auto &format : candidates) {
    const auto props{device.getFormatProperties(format)};
    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == vk::ImageTiling::eOptimal &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw VulkanKraftException("failed to find supported format");
}

vk::Format Context::_find_depth_format(const vk::PhysicalDevice &device) {
  return _find_supported_format(
      device,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void Context::_create_instance(
    const Window &window, const std::vector<const char *> &validation_layers) {

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

void Context::_setup_debug_messenger() {
  pfnVkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          m_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
  if (!pfnVkCreateDebugUtilsMessengerEXT) {
    throw VulkanKraftException("GetInstanceProcAddr: Unable to find "
                               "pfnVkCreateDebugUtilsMessengerEXT function.");
  }

  pfnVkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
          m_instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
  if (!pfnVkDestroyDebugUtilsMessengerEXT) {
    throw VulkanKraftException("GetInstanceProcAddr: Unable to find "
                               "pfnVkDestroyDebugUtilsMessengerEXT function.");
  }

  if constexpr (_enable_validation_layers) {
    vk::DebugUtilsMessengerCreateInfoEXT di;
    _populate_debug_messenger_create_info(di);

    try {
      m_debug_messenger = m_instance.createDebugUtilsMessengerEXT(di);
    } catch (const std::runtime_error &e) {
      throw VulkanKraftException(
          std::string("failed to set up debug messenger: ") + e.what());
    }
  }
}

void Context::_create_surface(const Window &window) {
  m_surface = window.create_vulkan_surface(m_instance);
}

void Context::_pick_physical_device(
    const std::vector<const char *> &extensions) {
  const auto devices(m_instance.enumeratePhysicalDevices());
  if (devices.empty()) {
    throw VulkanKraftException("no GPUs with vulkan support found");
  }

  for (const auto &d : devices) {
    if (_is_device_suitable(d, m_surface, extensions)) {
      m_physical_device = d;
    }
  }

  if (!m_physical_device) {
    throw VulkanKraftException("failed to find a suitable GPU");
  }
}

void Context::_create_logical_device(
    const std::vector<const char *> &extensions,
    const std::vector<const char *> &validation_layers) {
  const QueueFamilyIndices indices(m_physical_device, m_surface);
  std::vector<vk::DeviceQueueCreateInfo> qis;
  std::set<uint32_t> unique_queue_families = {indices.graphics_family.value(),
                                              indices.present_family.value()};

  const float queue_priority = 1.0f;
  for (const auto &qf : unique_queue_families) {
    vk::DeviceQueueCreateInfo qi;
    qi.queueFamilyIndex = qf;
    qi.queueCount = 1;
    qi.pQueuePriorities = &queue_priority;
    qis.emplace_back(std::move(qi));
  }

  vk::PhysicalDeviceFeatures features;
  features.samplerAnisotropy = VK_FALSE;
  features.sampleRateShading = VK_FALSE;

  vk::DeviceCreateInfo di;
  di.queueCreateInfoCount = static_cast<uint32_t>(qis.size());
  di.pQueueCreateInfos = qis.data();
  di.pEnabledFeatures = &features;
  di.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  di.ppEnabledExtensionNames = extensions.data();
  if constexpr (_enable_validation_layers) {
    di.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    di.ppEnabledLayerNames = validation_layers.data();
  }

  try {
    m_device = m_physical_device.createDevice(di);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create logical device: ") + e.what());
  }

  m_graphics_queue = m_device.getQueue(indices.graphics_family.value(), 0);
  m_present_queue = m_device.getQueue(indices.present_family.value(), 0);
}

void Context::_create_swap_chain(const Window &window) {
  m_swap_chain = std::make_unique<SwapChain>(m_physical_device, m_device,
                                             m_surface, m_render_pass, window);
}

void Context::_create_render_pass() {
  vk::AttachmentDescription col_at;
  col_at.format = m_swap_chain->get_image_format();
  col_at.samples = vk::SampleCountFlagBits::e1;
  col_at.loadOp = vk::AttachmentLoadOp::eClear;
  col_at.storeOp = vk::AttachmentStoreOp::eStore;
  col_at.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  col_at.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  col_at.initialLayout = vk::ImageLayout::eUndefined;
  col_at.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentDescription depth_at;
  depth_at.format = _find_depth_format(m_physical_device);
  depth_at.samples = vk::SampleCountFlagBits::e1;
  depth_at.loadOp = vk::AttachmentLoadOp::eClear;
  depth_at.storeOp = vk::AttachmentStoreOp::eDontCare;
  depth_at.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  depth_at.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  depth_at.initialLayout = vk::ImageLayout::eUndefined;
  depth_at.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::AttachmentReference col_at_ref;
  col_at_ref.attachment = 0;
  col_at_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentReference depth_at_ref;
  depth_at_ref.attachment = 1;
  depth_at_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::SubpassDescription sub;
  sub.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &col_at_ref;
  sub.pDepthStencilAttachment = &depth_at_ref;

  vk::SubpassDependency dep;
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                     vk::PipelineStageFlagBits::eEarlyFragmentTests;
  dep.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
  dep.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                     vk::PipelineStageFlagBits::eEarlyFragmentTests;
  dep.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                      vk::AccessFlagBits::eDepthStencilAttachmentWrite;

  const auto ats = std::array{col_at, depth_at};
  vk::RenderPassCreateInfo ri;
  ri.attachmentCount = static_cast<uint32_t>(ats.size());
  ri.pAttachments = ats.data();
  ri.subpassCount = 1;
  ri.pSubpasses = &sub;
  ri.dependencyCount = 1;
  ri.pDependencies = &dep;

  try {
    m_render_pass = m_device.createRenderPass(ri);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(std::string("failed to create render pass: ") +
                               e.what());
  }
}

} // namespace vulkan
} // namespace core