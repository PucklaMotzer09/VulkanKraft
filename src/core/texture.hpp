#pragma once
#include "vulkan/context.hpp"

namespace core {
class Texture {
public:
  class Builder {
  public:
    friend class Texture;

    Builder();

    inline Builder &dimensions(const uint32_t width, const uint32_t height) {
      m_width = width;
      m_height = height;
      return *this;
    }

    inline Builder &filter(const vk::Filter filter) {
      m_filter = filter;
      return *this;
    }

    inline Builder &address_mode(const vk::SamplerAddressMode address_mode) {
      m_address_mode = address_mode;
      return *this;
    }

    inline Builder &anisotropy(const float max_anisotropy) {
      m_max_anisotropy = max_anisotropy;
      return *this;
    }

    inline Builder &border_color(const vk::BorderColor border_color) {
      m_border_color = border_color;
      return *this;
    }

    inline Builder &mip_maps() {
      m_mip_levels = 2;
      return *this;
    }

    inline Builder &mip_map_mode(const vk::SamplerMipmapMode mip_mode) {
      m_mip_mode = mip_mode;
      return *this;
    }

    Texture build(const vulkan::Context &context, const void *data);

  private:
    uint32_t m_width;
    uint32_t m_height;
    vk::Filter m_filter;
    vk::SamplerAddressMode m_address_mode;
    float m_max_anisotropy;
    vk::BorderColor m_border_color;
    uint32_t m_mip_levels;
    vk::SamplerMipmapMode m_mip_mode;
  };

  ~Texture();

  inline vk::DescriptorImageInfo create_descriptor_image_info() const {
    vk::DescriptorImageInfo ii;
    ii.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    ii.imageView = m_image_view;
    ii.sampler = m_sampler;
    return ii;
  }

private:
  Texture(const vulkan::Context &context, const Builder &builder,
          const void *data);

  void _create_image(const Builder &builder, const void *data);
  void _create_image_view(const Builder &builder);
  void _create_sampler(const Builder &builder);
  void _generate_mip_maps(const Builder &builder);

  vk::Image m_image;
  vk::ImageView m_image_view;
  vk::DeviceMemory m_memory;
  vk::Sampler m_sampler;

  const vulkan::Context &m_context;
};
} // namespace core
