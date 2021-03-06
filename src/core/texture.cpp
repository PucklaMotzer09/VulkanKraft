#include "texture.hpp"
#include "exception.hpp"
#include <algorithm>
#include <cmath>

namespace core {
Texture::Builder::Builder()
    : m_width(0), m_height(0), m_filter(vk::Filter::eLinear),
      m_address_mode(vk::SamplerAddressMode::eRepeat), m_max_anisotropy(0.0f),
      m_border_color(vk::BorderColor::eIntOpaqueBlack), m_mip_levels(1),
      m_mip_mode(vk::SamplerMipmapMode::eLinear),
      m_format(vk::Format::eR8G8B8A8Srgb) {}

Texture Texture::Builder::build(const vulkan::Context &context,
                                const void *data) {
  if (m_width == 0) {
    throw VulkanKraftException("width of core::Texture cannot be 0");
  }
  if (m_height == 0) {
    throw VulkanKraftException("height of core::Texture cannot be 0");
  }

  if (m_mip_levels == 2) {
    m_mip_levels = static_cast<uint32_t>(
                       std::floor(std::log2(std::max(m_width, m_height)))) +
                   1;
  }

  return Texture(context, *this, data);
}

Texture::Texture(Texture &&rhs)
    : m_image(std::move(rhs.m_image)),
      m_image_view(std::move(rhs.m_image_view)),
      m_memory(std::move(rhs.m_memory)), m_sampler(std::move(rhs.m_sampler)),
      m_dynamic_sets(std::move(rhs.m_dynamic_sets)),
      m_dynamic_pool(rhs.m_dynamic_pool),
      m_dynamic_writes_to_perform(std::move(rhs.m_dynamic_writes_to_perform)),
      m_dynamic_binding_point(rhs.m_dynamic_binding_point),
      m_width(rhs.m_width), m_height(rhs.m_height), m_context(rhs.m_context) {
  rhs.m_dynamic_sets.clear();
  rhs.m_dynamic_pool = VK_NULL_HANDLE;
  rhs.m_dynamic_writes_to_perform.clear();
  rhs.m_image = VK_NULL_HANDLE;
  rhs.m_image_view = VK_NULL_HANDLE;
  rhs.m_memory = VK_NULL_HANDLE;
  rhs.m_sampler = VK_NULL_HANDLE;
  rhs.m_dynamic_binding_point = -1;
}

Texture &Texture::operator=(Texture &&rhs) {
  _destroy();

  m_image = std::move(rhs.m_image);
  m_image_view = std::move(rhs.m_image_view);
  m_memory = std::move(rhs.m_memory);
  m_sampler = std::move(rhs.m_sampler);
  m_dynamic_sets = std::move(rhs.m_dynamic_sets);
  m_dynamic_pool = rhs.m_dynamic_pool;
  m_dynamic_writes_to_perform = std::move(rhs.m_dynamic_writes_to_perform);
  m_dynamic_binding_point = rhs.m_dynamic_binding_point;
  m_width = rhs.m_width;
  m_height = rhs.m_height;

  rhs.m_image = VK_NULL_HANDLE;
  rhs.m_image_view = VK_NULL_HANDLE;
  rhs.m_memory = VK_NULL_HANDLE;
  rhs.m_sampler = VK_NULL_HANDLE;
  rhs.m_dynamic_sets.clear();
  rhs.m_dynamic_pool = VK_NULL_HANDLE;
  rhs.m_dynamic_writes_to_perform.clear();
  rhs.m_dynamic_binding_point = -1;

  return *this;
}

Texture::~Texture() { _destroy(); }

void Texture::rebuild(const Texture::Builder &builder, const void *data) {
  _destroy();

  _create_image(builder, data);
  if (builder.m_mip_levels > 1) {
    _generate_mip_maps(builder);
  }
  _create_image_view(builder);
  _create_sampler(builder);
}

uint32_t Texture::_get_image_size(const uint32_t width, const uint32_t height,
                                  const vk::Format format) {
  switch (format) {
  case vk::Format::eR8G8B8A8Srgb:
  case vk::Format::eR32Sfloat:
    return width * height * 4;
  case vk::Format::eR8G8B8Srgb:
    return width * height * 3;
  case vk::Format::eR8Srgb:
    return width * height;
  default:
    throw VulkanKraftException("The given format " +
                               std::to_string(static_cast<int>(format)) +
                               " is not supported by core::Texture");
  };
}

Texture::Texture(const vulkan::Context &context,
                 const Texture::Builder &builder, const void *data)
    : m_dynamic_binding_point(-1), m_dynamic_pool(VK_NULL_HANDLE),
      m_width(builder.m_width), m_height(builder.m_height), m_context(context) {
  _create_image(builder, data);
  if (builder.m_mip_levels > 1) {
    _generate_mip_maps(builder);
  }
  _create_image_view(builder);
  _create_sampler(builder);
}

void Texture::_create_image(const Texture::Builder &builder, const void *data) {
  // Create Image
  vk::ImageCreateInfo ii;
  ii.imageType = vk::ImageType::e2D;
  ii.extent.width = builder.m_width;
  ii.extent.height = builder.m_height;
  ii.extent.depth = 1;
  ii.mipLevels = builder.m_mip_levels;
  ii.arrayLayers = 1;
  ii.format = builder.m_format;
  ii.tiling = vk::ImageTiling::eOptimal;
  ii.initialLayout = vk::ImageLayout::eUndefined;
  ii.usage =
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
  if (builder.m_mip_levels > 1)
    ii.usage |= vk::ImageUsageFlagBits::eTransferSrc;
  ii.samples = vk::SampleCountFlagBits::e1;
  ii.sharingMode = vk::SharingMode::eExclusive;

  try {
    m_image = m_context.get_device().createImage(ii);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create image of core::Texture: ") + e.what());
  }

  // Allocate Memory
  auto mem_req(m_context.get_device().getImageMemoryRequirements(m_image));

  vk::MemoryAllocateInfo ai;
  ai.allocationSize = mem_req.size;
  ai.memoryTypeIndex = m_context.find_memory_type(
      mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

  try {
    m_memory = m_context.get_device().allocateMemory(ai);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to allocate memory for core::Texture: ") +
        e.what());
  }

  m_context.get_device().bindImageMemory(m_image, m_memory, 0);

  const auto image_size{
      _get_image_size(builder.m_width, builder.m_height, builder.m_format)};

  // Create Staging Buffer
  vk::BufferCreateInfo bi;
  bi.size = image_size;
  bi.usage = vk::BufferUsageFlagBits::eTransferSrc;
  bi.sharingMode = vk::SharingMode::eExclusive;

  vk::Buffer staging_buffer;
  try {
    staging_buffer = m_context.get_device().createBuffer(bi);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create staging buffer for core::Texture: ") +
        e.what());
  }

  mem_req = m_context.get_device().getBufferMemoryRequirements(staging_buffer);

  ai.allocationSize = mem_req.size;
  ai.memoryTypeIndex = m_context.find_memory_type(
      mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible |
                                  vk::MemoryPropertyFlagBits::eHostCoherent);

  vk::DeviceMemory staging_buffer_memory;
  try {
    staging_buffer_memory = m_context.get_device().allocateMemory(ai);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string(
            "failed to allocate memory for staging buffer of core::Texture: ") +
        e.what());
  }

  m_context.get_device().bindBufferMemory(staging_buffer, staging_buffer_memory,
                                          0);

  // Write data to staging buffer
  try {
    void *mapped_data =
        m_context.get_device().mapMemory(staging_buffer_memory, 0, image_size);
    memcpy(mapped_data, data, image_size);
    m_context.get_device().unmapMemory(staging_buffer_memory);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to write to staging buffer of core::Texture: ") +
        e.what());
  }

  // Transistion image layout
  m_context.transition_image_layout(
      m_image, builder.m_format, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eTransferDstOptimal, builder.m_mip_levels);

  // Copy staging buffer contents to image
  try {
    auto com_buf = m_context.begin_single_time_graphics_commands();

    vk::BufferImageCopy cp;
    cp.bufferOffset = 0;
    cp.bufferRowLength = 0;
    cp.bufferImageHeight = 0;

    cp.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    cp.imageSubresource.mipLevel = 0;
    cp.imageSubresource.baseArrayLayer = 0;
    cp.imageSubresource.layerCount = 1;

    cp.imageOffset = vk::Offset3D{0, 0, 0};
    cp.imageExtent = vk::Extent3D{builder.m_width, builder.m_height, 1};

    com_buf.copyBufferToImage(staging_buffer, m_image,
                              vk::ImageLayout::eTransferDstOptimal, cp);

    m_context.end_single_time_graphics_commands(std::move(com_buf));
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string(
            "failed to copy staging buffer to image of core::Texture: ") +
        e.what());
  }

  // Destroy staging buffer
  m_context.get_device().destroyBuffer(staging_buffer);
  m_context.get_device().freeMemory(staging_buffer_memory);

  if (builder.m_mip_levels == 1) {
    // Transistion to shader read only layout
    m_context.transition_image_layout(
        m_image, builder.m_format, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, 1);
  }
}

void Texture::_create_image_view(const Texture::Builder &builder) {
  vk::ImageViewCreateInfo vi;
  vi.image = m_image;
  vi.viewType = vk::ImageViewType::e2D;
  vi.format = builder.m_format;
  vi.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  vi.subresourceRange.baseMipLevel = 0;
  vi.subresourceRange.levelCount = builder.m_mip_levels;
  vi.subresourceRange.baseArrayLayer = 0;
  vi.subresourceRange.layerCount = 1;

  try {
    m_image_view = m_context.get_device().createImageView(vi);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create image view of core::Texture: ") +
        e.what());
  }
}

void Texture::_create_sampler(const Texture::Builder &builder) {
  vk::SamplerCreateInfo si;
  si.magFilter = builder.m_filter;
  si.minFilter = builder.m_filter;
  si.addressModeU = builder.m_address_mode;
  si.addressModeV = builder.m_address_mode;
  si.addressModeW = builder.m_address_mode;
  si.anisotropyEnable = builder.m_max_anisotropy != 0.0f;

  const auto &device_info(m_context.get_physical_device_info());
  si.maxAnisotropy =
      std::min(builder.m_max_anisotropy,
               device_info.properties.limits.maxSamplerAnisotropy);
  si.borderColor = builder.m_border_color;
  si.unnormalizedCoordinates = VK_FALSE;
  si.compareEnable = VK_FALSE;
  si.compareOp = vk::CompareOp::eAlways;
  si.mipmapMode = vk::SamplerMipmapMode::eLinear;
  si.mipLodBias = 0.0f;
  si.minLod = 0.0f;
  si.maxLod = static_cast<float>(builder.m_mip_levels);

  try {
    m_sampler = m_context.get_device().createSampler(si);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create sampler of core::Texture: ") + e.what());
  }
}

void Texture::_generate_mip_maps(const Builder &builder) {
  if (!m_context.get_physical_device_info().linear_blitting_support) {
    throw VulkanKraftException(
        "unable to generate mip maps for core::Texture because this device "
        "does not support linear blitting");
  }

  auto com_buf = m_context.begin_single_time_graphics_commands();

  vk::ImageMemoryBarrier bar;
  bar.image = m_image;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  bar.subresourceRange.baseArrayLayer = 0;
  bar.subresourceRange.layerCount = 1;
  bar.subresourceRange.levelCount = 1;

  auto mip_width{static_cast<int32_t>(builder.m_width)},
      mip_height{static_cast<int32_t>(builder.m_height)};

  for (uint32_t i = 1; i < builder.m_mip_levels; i++) {
    bar.subresourceRange.baseMipLevel = i - 1;
    bar.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    bar.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    bar.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    bar.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    com_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eTransfer,
                            static_cast<vk::DependencyFlagBits>(0), nullptr,
                            nullptr, bar);

    vk::ImageBlit b;
    b.srcOffsets[0] = vk::Offset3D{0, 0, 0};
    b.srcOffsets[1] = vk::Offset3D{mip_width, mip_height, 1};
    b.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    b.srcSubresource.mipLevel = i - 1;
    b.srcSubresource.baseArrayLayer = 0;
    b.srcSubresource.layerCount = 1;
    b.dstOffsets[0] = vk::Offset3D{0, 0, 0};
    b.dstOffsets[1] = vk::Offset3D{mip_width > 1 ? mip_width / 2 : 1,
                                   mip_height > 1 ? mip_height / 2 : 1, 1};
    b.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    b.dstSubresource.mipLevel = i;
    b.dstSubresource.baseArrayLayer = 0;
    b.dstSubresource.layerCount = 1;

    com_buf.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image,
                      vk::ImageLayout::eTransferDstOptimal, b,
                      vk::Filter::eLinear);

    bar.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    bar.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    bar.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    bar.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    com_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader,
                            static_cast<vk::DependencyFlagBits>(0), nullptr,
                            nullptr, bar);

    if (mip_width > 1) {
      mip_width /= 2;
    }
    if (mip_height > 1) {
      mip_height /= 2;
    }
  }

  bar.subresourceRange.baseMipLevel = builder.m_mip_levels - 1;
  bar.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  bar.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  bar.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  bar.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  com_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                          vk::PipelineStageFlagBits::eFragmentShader,
                          static_cast<vk::DependencyFlagBits>(0), nullptr,
                          nullptr, bar);

  m_context.end_single_time_graphics_commands(com_buf);
}

void Texture::_create_descriptor_sets(const vk::DescriptorPool &pool,
                                      const vk::DescriptorSetLayout &layout,
                                      const uint32_t binding_point) {
  m_dynamic_binding_point = binding_point;
  if (!m_dynamic_sets.empty())
    return;

  const std::vector<vk::DescriptorSetLayout> layouts(
      m_context.get_swap_chain_image_count(), layout);

  vk::DescriptorSetAllocateInfo ai;
  ai.descriptorPool = pool;
  ai.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  ai.pSetLayouts = layouts.data();
  try {
    m_dynamic_sets = m_context.get_device().allocateDescriptorSets(ai);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string(
            "failed to allocate dynamic descriptor sets of core::Texture: ") +
        e.what());
  }

  m_dynamic_pool = pool;

  m_dynamic_writes_to_perform.clear();
  for (uint32_t i = 0; i < m_dynamic_sets.size(); i++) {
    m_dynamic_writes_to_perform.emplace(i);
  }
}

void Texture::_write_dynamic_set(const size_t index) {
  if (m_dynamic_writes_to_perform.count(index) == 0) {
    return;
  }

  const auto image_info(create_descriptor_image_info());
  vk::WriteDescriptorSet write;
  write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  write.descriptorCount = 1;
  write.dstArrayElement = 0;
  write.dstBinding = m_dynamic_binding_point;
  write.dstSet = m_dynamic_sets[index];
  write.pImageInfo = &image_info;

  m_context.get_device().updateDescriptorSets(write, nullptr);

  m_dynamic_writes_to_perform.erase(index);
}

void Texture::_destroy() {
  if (m_sampler)
    m_context.get_device().destroySampler(m_sampler);
  if (m_image_view)
    m_context.get_device().destroyImageView(m_image_view);
  if (m_image)
    m_context.get_device().destroyImage(m_image);
  if (m_memory)
    m_context.get_device().freeMemory(m_memory);

  if (!m_dynamic_sets.empty()) {
    try {
      m_context.get_device().freeDescriptorSets(m_dynamic_pool, m_dynamic_sets);
    } catch (const std::runtime_error &e) {
      throw VulkanKraftException(
          std::string("failed to free dynamic sets of core::Texture: ") +
          e.what());
    }
  }
}

} // namespace core
