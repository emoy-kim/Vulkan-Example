#pragma once

#include "vulkan_model.h"

struct PipelineConfiguration
{
   VkPipelineViewportStateCreateInfo ViewportInfo;
   VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo;
   VkPipelineRasterizationStateCreateInfo RasterizationInfo;
   VkPipelineMultisampleStateCreateInfo MultisampleInfo;
   VkPipelineColorBlendAttachmentState ColorBlendAttachment;
   VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
   VkPipelineDepthStencilStateCreateInfo DepthStencilInfo;
   std::vector<VkDynamicState> DynamicStatieEnables;
   VkPipelineDynamicStateCreateInfo DynamicStateInfo;
   VkPipelineLayout PipelineLayout;
   VkRenderPass RenderPass;
   uint32_t Subpass;

   PipelineConfiguration() :
      ViewportInfo{}, InputAssemblyInfo{}, RasterizationInfo{}, MultisampleInfo{}, ColorBlendAttachment{},
      ColorBlendInfo{}, DepthStencilInfo{}, DynamicStateInfo{}, PipelineLayout( nullptr ), RenderPass( nullptr ),
      Subpass( 0 ) {}
};

class PipelineVK
{
public:
   PipelineVK(
      DeviceVK* device,
      const std::string& vertex_shader_file,
      const std::string& fragment_shader_file,
      const PipelineConfiguration& configuration
   );
   ~PipelineVK();

   PipelineVK(const PipelineVK&) = delete;
   PipelineVK(const PipelineVK&&) = delete;
   PipelineVK& operator=(const PipelineVK&) = delete;
   PipelineVK& operator=(const PipelineVK&&) = delete;

   static void getDefaultPipelineConfiguration(PipelineConfiguration& configuration);
   void bind(VkCommandBuffer command_buffer);

private:
   DeviceVK* Device;
   VkPipeline GraphicsPipeline;
   VkShaderModule VertexShaderModule;
   VkShaderModule FragmentShaderModule;

   static std::vector<char> readFile(const std::string& file_path);
   void createGraphicsPipeline(
      const std::string& vertex_shader_file,
      const std::string& fragment_shader_file,
      const PipelineConfiguration& configuration
   );
   void createShaderModule(const std::vector<char>& code, VkShaderModule* module);
};