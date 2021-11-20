#include "vulkan_pipeline.h"

PipelineVK::PipelineVK(
   DeviceVK* device,
   const std::string& vertex_shader_file,
   const std::string& fragment_shader_file,
   const PipelineConfiguration& configuration
) : Device( device ), GraphicsPipeline{}, VertexShaderModule{}, FragmentShaderModule{}
{
   createGraphicsPipeline( vertex_shader_file, fragment_shader_file, configuration );
}

PipelineVK::~PipelineVK()
{
   vkDestroyShaderModule( Device->device(), VertexShaderModule, nullptr );
   vkDestroyShaderModule( Device->device(), FragmentShaderModule, nullptr );
   vkDestroyPipeline( Device->device(), GraphicsPipeline, nullptr );
}

std::vector<char> PipelineVK::readFile(const std::string& file_path)
{
   std::ifstream file(file_path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      std::cout << "Could not open the file, " << file_path << "\n";
      return std::vector<char>{};
   }

   auto file_size = static_cast<int>(file.tellg());
   std::vector<char> buffer(file_size);
   file.seekg( 0 );
   file.read( buffer.data(), file_size );
   file.close();
   return buffer;
}

void PipelineVK::createGraphicsPipeline(
   const std::string& vertex_shader_file,
   const std::string& fragment_shader_file,
   const PipelineConfiguration& configuration
)
{
   assert( configuration.PipelineLayout != VK_NULL_HANDLE );
   assert( configuration.RenderPass != VK_NULL_HANDLE );

   auto vertex_code = readFile( vertex_shader_file );
   auto fragment_code = readFile( fragment_shader_file );
   createShaderModule( vertex_code, &VertexShaderModule );
   createShaderModule( fragment_code, &FragmentShaderModule );

   VkPipelineShaderStageCreateInfo shader_stages[2];
   shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].module = VertexShaderModule;
   shader_stages[0].pName = "main";
   shader_stages[0].flags = 0;
   shader_stages[0].pNext = nullptr;
   shader_stages[0].pSpecializationInfo = nullptr;

   shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].module = FragmentShaderModule;
   shader_stages[1].pName = "main";
   shader_stages[1].flags = 0;
   shader_stages[1].pNext = nullptr;
   shader_stages[1].pSpecializationInfo = nullptr;

   VkPipelineVertexInputStateCreateInfo vertex_input_info{};
   auto binding_description = ModelVK::Vertex::getBindingDescriptions();
   auto attribute_description = ModelVK::Vertex::getAttributeDescriptions();
   vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
   vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_description.size());
   vertex_input_info.pVertexAttributeDescriptions = attribute_description.data();
   vertex_input_info.pVertexBindingDescriptions = binding_description.data();

   VkGraphicsPipelineCreateInfo pipeline_info{};
   pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipeline_info.stageCount = 2;
   pipeline_info.pStages = shader_stages;
   pipeline_info.pVertexInputState = &vertex_input_info;
   pipeline_info.pInputAssemblyState = &configuration.InputAssemblyInfo;
   pipeline_info.pViewportState = &configuration.ViewportInfo;
   pipeline_info.pRasterizationState = &configuration.RasterizationInfo;
   pipeline_info.pMultisampleState = &configuration.MultisampleInfo;
   pipeline_info.pColorBlendState = &configuration.ColorBlendInfo;
   pipeline_info.pDepthStencilState = &configuration.DepthStencilInfo;
   pipeline_info.pDynamicState = &configuration.DynamicStateInfo;

   pipeline_info.layout = configuration.PipelineLayout;
   pipeline_info.renderPass = configuration.RenderPass;
   pipeline_info.subpass = configuration.Subpass;

   pipeline_info.basePipelineIndex = -1;
   pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

   if (vkCreateGraphicsPipelines( Device->device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &GraphicsPipeline ) != VK_SUCCESS) {
      std::cout << "Could not create graphics pipeline\n";
   }
}

void PipelineVK::createShaderModule(const std::vector<char>& code, VkShaderModule* module)
{
   VkShaderModuleCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   create_info.codeSize = code.size();
   create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

   if (vkCreateShaderModule( Device->device(), &create_info, nullptr, module ) != VK_SUCCESS) {
      std::cout << "Could not create shader module\n";
      return;
   }
}

void PipelineVK::getDefaultPipelineConfiguration(PipelineConfiguration& configuration)
{
   configuration.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   configuration.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   configuration.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

   configuration.ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   configuration.ViewportInfo.viewportCount = 1;
   configuration.ViewportInfo.pViewports = nullptr;
   configuration.ViewportInfo.scissorCount = 1;
   configuration.ViewportInfo.pScissors = nullptr;

   configuration.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   configuration.RasterizationInfo.depthClampEnable = VK_FALSE;
   configuration.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
   configuration.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
   configuration.RasterizationInfo.lineWidth = 1.0f;
   configuration.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
   configuration.RasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
   configuration.RasterizationInfo.depthBiasEnable = VK_FALSE;
   configuration.RasterizationInfo.depthBiasConstantFactor = 0.0f; // optional
   configuration.RasterizationInfo.depthBiasClamp = 0.0f; // optional
   configuration.RasterizationInfo.depthBiasSlopeFactor = 0.0f; // optional

   configuration.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   configuration.MultisampleInfo.sampleShadingEnable = VK_FALSE;
   configuration.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   configuration.MultisampleInfo.minSampleShading = 1.0f; // optional
   configuration.MultisampleInfo.pSampleMask = nullptr; // optional
   configuration.MultisampleInfo.alphaToCoverageEnable = VK_FALSE; // optional
   configuration.MultisampleInfo.alphaToOneEnable = VK_FALSE; // optional

   configuration.ColorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   configuration.ColorBlendAttachment.blendEnable = VK_FALSE;
   configuration.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // optional
   configuration.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
   configuration.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // optional
   configuration.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // optional
   configuration.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
   configuration.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // optional

   configuration.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   configuration.ColorBlendInfo.logicOpEnable = VK_FALSE;
   configuration.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // optional
   configuration.ColorBlendInfo.attachmentCount = 1;
   configuration.ColorBlendInfo.pAttachments = &configuration.ColorBlendAttachment;
   configuration.ColorBlendInfo.blendConstants[0] = 0.0f; // optional
   configuration.ColorBlendInfo.blendConstants[1] = 0.0f; // optional
   configuration.ColorBlendInfo.blendConstants[2] = 0.0f; // optional
   configuration.ColorBlendInfo.blendConstants[3] = 0.0f; // optional

   configuration.DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   configuration.DepthStencilInfo.depthTestEnable = VK_TRUE;
   configuration.DepthStencilInfo.depthWriteEnable = VK_TRUE;
   configuration.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
   configuration.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
   configuration.DepthStencilInfo.minDepthBounds = 0.0f; // optional
   configuration.DepthStencilInfo.maxDepthBounds = 1.0f; // optional
   configuration.DepthStencilInfo.stencilTestEnable = VK_FALSE;
   configuration.DepthStencilInfo.front = {}; // optional
   configuration.DepthStencilInfo.back = {}; // optional

   configuration.DynamicStatieEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   configuration.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   configuration.DynamicStateInfo.pDynamicStates = configuration.DynamicStatieEnables.data();
   configuration.DynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configuration.DynamicStatieEnables.size());
   configuration.DynamicStateInfo.flags = 0;
}

void PipelineVK::bind(VkCommandBuffer command_buffer)
{
   vkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline );
}