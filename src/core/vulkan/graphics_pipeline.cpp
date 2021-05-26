#include "graphics_pipeline.hpp"
#include "../exception.hpp"
#include "vertex.hpp"
#include <array>

namespace core {
namespace vulkan {
GraphicsPipeline::GraphicsPipeline(const Context &context,
                                   std::vector<char> vertex_code,
                                   std::vector<char> fragment_code)
    : m_context(context) {
  _create_handle(std::move(vertex_code), std::move(fragment_code));
}

GraphicsPipeline::~GraphicsPipeline() { _destroy(); }

void GraphicsPipeline::bind() {
  if (!m_context.m_swap_chain->get_current_image()) {
    return;
  }
  m_context
      .m_graphic_command_buffers[m_context.m_swap_chain->get_current_image()
                                     .value()]
      .bindPipeline(vk::PipelineBindPoint::eGraphics, m_handle);
}

vk::ShaderModule
GraphicsPipeline::_create_shader_module(const vk::Device &device,
                                        std::vector<char> shader_code) {
  vk::ShaderModuleCreateInfo si;
  si.codeSize = shader_code.size();
  si.pCode = reinterpret_cast<const uint32_t *>(shader_code.data());

  try {
    return device.createShaderModule(si);
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(std::string("failed to create shader module: ") +
                               e.what());
  }
}

void GraphicsPipeline::_create_handle(std::vector<char> vertex_code,
                                      std::vector<char> fragment_code) {
  try {
    m_vertex_module =
        _create_shader_module(m_context.m_device, std::move(vertex_code));
  } catch (const VulkanKraftException &e) {
    throw VulkanKraftException(
        std::string("failed to create vertex shader module: ") + e.what());
  }

  try {
    m_fragment_module =
        _create_shader_module(m_context.m_device, std::move(fragment_code));
  } catch (const VulkanKraftException &e) {
    throw VulkanKraftException(
        std::string("failed to create fragment shader module: ") + e.what());
  }

  vk::PipelineShaderStageCreateInfo vert_i;
  vert_i.stage = vk::ShaderStageFlagBits::eVertex;
  vert_i.module = m_vertex_module;
  vert_i.pName = _shader_function_name;

  vk::PipelineShaderStageCreateInfo frag_i;
  frag_i.stage = vk::ShaderStageFlagBits::eFragment;
  frag_i.module = m_fragment_module;
  frag_i.pName = _shader_function_name;

  const auto shader_stages = std::array{vert_i, frag_i};

  const auto bind_desc(Vertex::get_binding_description());
  const auto att_desc(Vertex::get_attribute_description());
  vk::PipelineVertexInputStateCreateInfo vi_i;
  vi_i.vertexBindingDescriptionCount = 1;
  vi_i.vertexAttributeDescriptionCount = static_cast<uint32_t>(att_desc.size());
  vi_i.pVertexBindingDescriptions = &bind_desc;
  vi_i.pVertexAttributeDescriptions = att_desc.data();

  vk::PipelineInputAssemblyStateCreateInfo ia_i;
  ia_i.topology = vk::PrimitiveTopology::eTriangleList;
  ia_i.primitiveRestartEnable = VK_FALSE;

  vk::PipelineViewportStateCreateInfo vs_i;
  vs_i.viewportCount = 1;
  vs_i.scissorCount = 1;

  vk::PipelineRasterizationStateCreateInfo rast_i;
  rast_i.depthClampEnable = VK_FALSE;
  rast_i.rasterizerDiscardEnable = VK_FALSE;
  rast_i.polygonMode = vk::PolygonMode::eFill;
  rast_i.lineWidth = 1.0f;
  rast_i.cullMode = vk::CullModeFlagBits::eBack;
  rast_i.frontFace = vk::FrontFace::eCounterClockwise;
  rast_i.depthBiasEnable = VK_FALSE;

  vk::PipelineMultisampleStateCreateInfo multi_i;
  multi_i.sampleShadingEnable = VK_FALSE;
  multi_i.rasterizationSamples = vk::SampleCountFlagBits::e1;

  vk::PipelineColorBlendAttachmentState col_blend_at;
  col_blend_at.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  col_blend_at.blendEnable = VK_FALSE;

  vk::PipelineColorBlendStateCreateInfo cb_i;
  cb_i.logicOpEnable = VK_FALSE;
  cb_i.logicOp = vk::LogicOp::eCopy;
  cb_i.attachmentCount = 1;
  cb_i.pAttachments = &col_blend_at;
  cb_i.blendConstants.fill(0.0f);

  if (!m_layout) {
    vk::PipelineLayoutCreateInfo pl_i;
    pl_i.setLayoutCount = 0;
    pl_i.pushConstantRangeCount = 0;

    try {
      m_layout = m_context.m_device.createPipelineLayout(pl_i);
    } catch (const std::runtime_error &e) {
      throw VulkanKraftException(
          std::string("failed to create pipeline layout: ") + e.what());
    }
  }

  vk::PipelineDepthStencilStateCreateInfo ds_i;
  ds_i.depthTestEnable = VK_TRUE;
  ds_i.depthWriteEnable = VK_TRUE;
  ds_i.depthCompareOp = vk::CompareOp::eLess;
  ds_i.depthBoundsTestEnable = VK_FALSE;
  ds_i.minDepthBounds = 0.0f;
  ds_i.maxDepthBounds = 1.0f;
  ds_i.stencilTestEnable = VK_FALSE;

  const auto dyn_states =
      std::array{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dyn_i;
  dyn_i.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
  dyn_i.pDynamicStates = dyn_states.data();

  vk::GraphicsPipelineCreateInfo p_i;
  p_i.stageCount = static_cast<uint32_t>(shader_stages.size());
  p_i.pStages = shader_stages.data();
  p_i.pVertexInputState = &vi_i;
  p_i.pInputAssemblyState = &ia_i;
  p_i.pViewportState = &vs_i;
  p_i.pRasterizationState = &rast_i;
  p_i.pMultisampleState = &multi_i;
  p_i.pColorBlendState = &cb_i;
  p_i.pDepthStencilState = &ds_i;
  p_i.pDynamicState = &dyn_i;
  p_i.layout = m_layout;
  p_i.renderPass = m_context.m_swap_chain->get_render_pass();
  p_i.subpass = 0;
  p_i.basePipelineHandle = VK_NULL_HANDLE;

  try {
    const auto ps =
        m_context.m_device.createGraphicsPipelines(VK_NULL_HANDLE, p_i);
    if (ps.result != vk::Result::eSuccess) {
      throw std::runtime_error(std::to_string(static_cast<int>(ps.result)));
    }
    m_handle = ps.value[0];
  } catch (const std::runtime_error &e) {
    throw VulkanKraftException(
        std::string("failed to create graphics pipeline: ") + e.what());
  }
}

void GraphicsPipeline::_destroy() {
  m_context.m_device.waitIdle();

  m_context.m_device.destroyPipeline(m_handle);
  m_context.m_device.destroyPipelineLayout(m_layout);
  m_context.m_device.destroyShaderModule(m_vertex_module);
  m_context.m_device.destroyShaderModule(m_fragment_module);
}

} // namespace vulkan
} // namespace core