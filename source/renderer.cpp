#include "renderer.h"

RendererVK::RendererVK() :
   Window( nullptr ), Instance{}, Surface{}, PhysicalDevice( VK_NULL_HANDLE ), Device{}, GraphicsQueue{},
   PresentQueue{}, SwapChain{}, SwapChainImageFormat{}, SwapChainExtent{}, RenderPass{}, DescriptorSetLayout{},
   PipelineLayout{}, GraphicsPipeline{}, CommandPool{}, DepthImage{}, DepthImageMemory{}, DepthImageView{},
   TextureImage{}, TextureImageMemory{}, TextureImageView{}, TextureSampler{}, VertexBuffer{}, VertexBufferMemory{},
   IndexBuffer{}, IndexBufferMemory{}, DescriptorPool{}, CurrentFrame( 0 ), FramebufferResized( false )
{

}

void RendererVK::play()
{
   initializeWindow();
   initializeVulkan();
   mainLoop();
   cleanup();
}

void RendererVK::initializeWindow()
{
   glfwInit();

   glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
   glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

   Window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );
}

#ifdef _DEBUG
void RendererVK::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
{
   create_info = {};
   create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   create_info.pfnUserCallback = debugCallback;
}

void RendererVK::setupDebugMessenger()
{
   VkDebugUtilsMessengerCreateInfoEXT create_info;
   populateDebugMessengerCreateInfo( create_info );

   const VkResult result = createDebugUtilsMessengerEXT(
   Instance,
   &create_info,
   nullptr,
   &DebugMessenger
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to set up debug messenger!");
}

bool RendererVK::checkValidationLayerSupport()
{
   uint32_t layer_count;
   vkEnumerateInstanceLayerProperties( &layer_count, nullptr );

   std::vector<VkLayerProperties> available_layers( layer_count);
   vkEnumerateInstanceLayerProperties( &layer_count, available_layers.data() );

   for (const char* layer_name : ValidationLayers) {
      bool layer_found = false;
      for (const auto& layerProperties : available_layers) {
          if (std::strcmp( layer_name, layerProperties.layerName ) == 0) {
             layer_found = true;
              break;
          }
      }
      if (!layer_found) return false;
   }
   return true;
}
#endif

void RendererVK::createSurface()
{
   if (glfwCreateWindowSurface( Instance, Window, nullptr, &Surface ) != VK_SUCCESS) {
      throw std::runtime_error("failed to create Window Surface!");
   }
}

RendererVK::QueueFamilyIndices RendererVK::findQueueFamilies(VkPhysicalDevice device)
{
   QueueFamilyIndices indices;
   uint32_t queue_family_count = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(
      device,
      &queue_family_count,
      nullptr
   );

   std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
   vkGetPhysicalDeviceQueueFamilyProperties(
   device,
   &queue_family_count,
   queue_families.data()
   );

   int i = 0;
   for (const auto& queue_family : queue_families) {
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.GraphicsFamily = i;

      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(
      device,
      i,
      Surface,
      &present_support
      );
      if (present_support) indices.PresentFamily = i;
      if (indices.isComplete()) break;
      i++;
   }
   return indices;
}

bool RendererVK::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
   uint32_t extension_count;
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extension_count,
      nullptr
   );

   std::vector<VkExtensionProperties> available_extensions(extension_count);
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extension_count,
      available_extensions.data()
   );

   std::set<std::string> required_extensions(DeviceExtensions.begin(), DeviceExtensions.end());
   for (const auto& extension : available_extensions) {
      required_extensions.erase( extension.extensionName );
   }
   return required_extensions.empty();
}

RendererVK::SwapChainSupportDetails RendererVK::querySwapChainSupport(VkPhysicalDevice device)
{
   SwapChainSupportDetails details;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device,
      Surface,
      &details.Capabilities
   );

   uint32_t format_count;
   vkGetPhysicalDeviceSurfaceFormatsKHR(
      device,
      Surface,
      &format_count,
      nullptr
   );
   if (format_count != 0) {
      details.Formats.resize( format_count );
      vkGetPhysicalDeviceSurfaceFormatsKHR(
         device,
         Surface,
         &format_count,
         details.Formats.data()
      );
   }

   uint32_t present_mode_count;
   vkGetPhysicalDeviceSurfacePresentModesKHR(
      device,
      Surface,
      &present_mode_count,
      nullptr
   );
   if (present_mode_count != 0) {
      details.PresentModes.resize( present_mode_count );
      vkGetPhysicalDeviceSurfacePresentModesKHR(
         device,
         Surface,
         &present_mode_count,
         details.PresentModes.data()
      );
   }
   return details;
}

bool RendererVK::isDeviceSuitable(VkPhysicalDevice device)
{
   QueueFamilyIndices indices = findQueueFamilies( device );
   if (!indices.isComplete()) return false;

   bool swap_chain_adequate = false;
   bool extensions_supported = checkDeviceExtensionSupport( device );
   if (extensions_supported) {
      SwapChainSupportDetails swap_chain_support = querySwapChainSupport( device );
      swap_chain_adequate = !swap_chain_support.Formats.empty() && !swap_chain_support.PresentModes.empty();
   }

   VkPhysicalDeviceFeatures supported_features;
   vkGetPhysicalDeviceFeatures( device, &supported_features );
   return extensions_supported && swap_chain_adequate && supported_features.samplerAnisotropy;
}

void RendererVK::pickPhysicalDevice()
{
   uint32_t device_count = 0;
   vkEnumeratePhysicalDevices( Instance, &device_count, nullptr );

   if (device_count == 0) throw std::runtime_error( "failed to find GPUs with Vulkan support!");

   std::vector<VkPhysicalDevice> devices( device_count);
   vkEnumeratePhysicalDevices(
      Instance,
      &device_count,
      devices.data()
   );

   for (const auto& device : devices) {
      if (isDeviceSuitable( device )) {
         PhysicalDevice = device;
         break;
      }
   }

   if (PhysicalDevice == VK_NULL_HANDLE) throw std::runtime_error("failed to find a suitable GPU!");
}

void RendererVK::createLogicalDevice()
{
   const float queue_priority = 1.0f;
   std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
   QueueFamilyIndices indices = findQueueFamilies( PhysicalDevice );
   std::set<uint32_t> unique_queue_families = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };
   for (uint32_t queue_family : unique_queue_families) {
      VkDeviceQueueCreateInfo queue_create_info{};
      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.queueFamilyIndex = queue_family;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back( queue_create_info );
   }

   VkPhysicalDeviceFeatures device_features{};
   // write later ...

   VkDeviceCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
   create_info.pQueueCreateInfos = queue_create_infos.data();
   create_info.pEnabledFeatures = &device_features;
   create_info.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
   create_info.ppEnabledExtensionNames = DeviceExtensions.data();

#ifdef NDBUG
   create_info.enabledLayerCount = 0;
#else
   // Vulkan made a distinction between Instance and Device specific validation layers, but this is no longer the case.
   // That means that the enabledLayerCount and ppEnabledLayerNames fields are ignored by up-to-date implementations.
   // However, it is still a good idea to set them anyway to be compatible with older implementations.
   create_info.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
   create_info.ppEnabledLayerNames = ValidationLayers.data();
#endif

   const VkResult result = vkCreateDevice(
      PhysicalDevice,
      &create_info,
      nullptr,
      &Device
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create logical Device!");

   vkGetDeviceQueue(
      Device,
      indices.GraphicsFamily.value(),
      0,
      &GraphicsQueue
   );
   vkGetDeviceQueue(
      Device,
      indices.PresentFamily.value(),
      0,
      &PresentQueue
   );
}

VkSurfaceFormatKHR RendererVK::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
   for (const auto& available_format : available_formats) {
      if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
         return available_format;
      }
   }
   return available_formats[0];
}

VkPresentModeKHR RendererVK::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes)
{
   for (const auto& available_present_mode : available_present_modes) {
      if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) return available_present_mode;
   }
   return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RendererVK::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
   if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;
   else {
      int width, height;
      glfwGetFramebufferSize( Window, &width, &height );
      VkExtent2D actual_extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
      actual_extent.width = std::clamp(
         actual_extent.width,
         capabilities.minImageExtent.width, capabilities.maxImageExtent.width
      );
      actual_extent.height = std::clamp(
         actual_extent.height,
         capabilities.minImageExtent.height, capabilities.maxImageExtent.height
      );
      return actual_extent;
   }
}

void RendererVK::createSwapChain()
{
   SwapChainSupportDetails swap_chain_support = querySwapChainSupport( PhysicalDevice );
   VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat( swap_chain_support.Formats );
   VkPresentModeKHR present_mode = chooseSwapPresentMode( swap_chain_support.PresentModes );
   VkExtent2D extent = chooseSwapExtent( swap_chain_support.Capabilities );

   // Simply sticking to the minimum means that we may sometimes have to wait on the driver to complete internal
   // operations before we can acquire another image to render to. Therefore, it is recommended to request at least
   // one more image than the minimum.
   uint32_t image_count = swap_chain_support.Capabilities.minImageCount + 1;
   if (swap_chain_support.Capabilities.maxImageCount > 0 &&
       image_count > swap_chain_support.Capabilities.maxImageCount) {
      image_count = swap_chain_support.Capabilities.maxImageCount;
   }

   VkSwapchainCreateInfoKHR create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   create_info.surface = Surface;
   create_info.minImageCount = image_count;
   create_info.imageFormat = surface_format.format;
   create_info.imageColorSpace = surface_format.colorSpace;
   create_info.imageExtent = extent;
   create_info.imageArrayLayers = 1;
   create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   QueueFamilyIndices indices = findQueueFamilies( PhysicalDevice );
   std::array<uint32_t, 2> queue_family_indices = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };
   if (indices.GraphicsFamily != indices.PresentFamily) {
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = queue_family_indices.size();
      create_info.pQueueFamilyIndices = queue_family_indices.data();
   }
   else create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

   create_info.preTransform = swap_chain_support.Capabilities.currentTransform;
   create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   create_info.presentMode = present_mode;
   create_info.clipped = VK_TRUE;
   create_info.oldSwapchain = VK_NULL_HANDLE;

   const VkResult result = vkCreateSwapchainKHR(
      Device,
      &create_info,
      nullptr,
      &SwapChain
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create swap chain!");

   vkGetSwapchainImagesKHR(
      Device,
      SwapChain,
      &image_count,
      nullptr
   );
   SwapChainImages.resize( image_count );
   vkGetSwapchainImagesKHR(
      Device,
      SwapChain,
      &image_count,
      SwapChainImages.data()
   );
   SwapChainImageFormat = surface_format.format;
   SwapChainExtent = extent;
}

void RendererVK::createImageViews()
{
   SwapChainImageViews.resize( SwapChainImages.size() );
   for (size_t i = 0; i < SwapChainImages.size(); ++i) {
      VkImageViewCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = SwapChainImages[i];
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      create_info.format = SwapChainImageFormat;
      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      const VkResult result = vkCreateImageView(
         Device,
         &create_info,
         nullptr,
         &SwapChainImageViews[i]
      );
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create image views!");
   }
}

void RendererVK::createRenderPass()
{
   VkAttachmentDescription color_attachment{};
   color_attachment.format = SwapChainImageFormat;
   color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
   color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentDescription depth_attachment{};
   depth_attachment.format = findDepthFormat();
   depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
   depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkAttachmentReference color_attachment_ref{};
   color_attachment_ref.attachment = 0;
   color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkAttachmentReference depth_attachment_ref{};
   depth_attachment_ref.attachment = 1;
   depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass{};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &color_attachment_ref;
   subpass.pDepthStencilAttachment = &depth_attachment_ref;

   VkSubpassDependency dependency{};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.dstSubpass = 0;
   dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
   dependency.srcAccessMask = 0;
   dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
   dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

   std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
   VkRenderPassCreateInfo render_pass_info{};
   render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
   render_pass_info.pAttachments = attachments.data();
   render_pass_info.subpassCount = 1;
   render_pass_info.pSubpasses = &subpass;
   render_pass_info.dependencyCount = 1;
   render_pass_info.pDependencies = &dependency;

   const VkResult result = vkCreateRenderPass(
      Device,
      &render_pass_info,
      nullptr,
      &RenderPass
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create render pass!");
}

void RendererVK::createDescriptorSetLayout()
{
   VkDescriptorSetLayoutBinding ubo_layout_binding{};
   ubo_layout_binding.binding = 0;
   ubo_layout_binding.descriptorCount = 1;
   ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   ubo_layout_binding.pImmutableSamplers = nullptr;
   ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   VkDescriptorSetLayoutBinding sampler_layout_binding{};
   sampler_layout_binding.binding = 1;
   sampler_layout_binding.descriptorCount = 1;
   sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   sampler_layout_binding.pImmutableSamplers = nullptr;
   sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

   std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
   VkDescriptorSetLayoutCreateInfo layoutInfo{};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
   layoutInfo.pBindings = bindings.data();

   const VkResult result = vkCreateDescriptorSetLayout(
      Device,
      &layoutInfo,
      nullptr,
      &DescriptorSetLayout
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create descriptor set layout!");
}

std::vector<char> RendererVK::readFile(const std::string& filename)
{
   std::ifstream file(filename, std::ios::ate | std::ios::binary);

   if (!file.is_open()) throw std::runtime_error("failed to open file!");

   const auto file_size = static_cast<int>(file.tellg());
   std::vector<char> buffer( file_size);
   file.seekg( 0 );
   file.read( buffer.data(), file_size );
   file.close();
   return buffer;
}

VkShaderModule RendererVK::createShaderModule(const std::vector<char>& code)
{
   VkShaderModuleCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   create_info.codeSize = code.size();
   create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

   VkShaderModule shader_module;
   const VkResult result = vkCreateShaderModule(
      Device,
      &create_info,
      nullptr,
      &shader_module
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create shader module!");
   return shader_module;
}

void RendererVK::createGraphicsPipeline()
{
   std::vector<char> vert_shader_code = readFile( "../shaders/shader.vert.spv" );
   std::vector<char> frag_shader_code = readFile( "../shaders/shader.frag.spv" );

   VkShaderModule vert_shader_module = createShaderModule( vert_shader_code );
   VkShaderModule frag_shader_module = createShaderModule( frag_shader_code );

   VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
   vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vert_shader_stage_info.module = vert_shader_module;
   vert_shader_stage_info.pName = "main";

   VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
   frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   frag_shader_stage_info.module = frag_shader_module;
   frag_shader_stage_info.pName = "main";

   VkPipelineVertexInputStateCreateInfo vertex_input_info{};
   vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertex_input_info.vertexBindingDescriptionCount = 0;
   vertex_input_info.vertexAttributeDescriptionCount = 0;

   VkVertexInputBindingDescription binding_description = Vertex::getBindingDescription();
   std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions = Vertex::getAttributeDescriptions();
   vertex_input_info.vertexBindingDescriptionCount = 1;
   vertex_input_info.pVertexBindingDescriptions = &binding_description;
   vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
   vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

   VkPipelineInputAssemblyStateCreateInfo input_assembly{};
   input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   input_assembly.primitiveRestartEnable = VK_FALSE;

   VkViewport viewport{};
   viewport.x = 0.0f;
   viewport.y = 0.0f;
   viewport.width = static_cast<float>(SwapChainExtent.width);
   viewport.height = static_cast<float>(SwapChainExtent.height);
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;

   VkRect2D scissor{};
   scissor.offset = { 0, 0 };
   scissor.extent = SwapChainExtent;

   VkPipelineViewportStateCreateInfo viewport_state{};
   viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewport_state.viewportCount = 1;
   viewport_state.pViewports = &viewport;
   viewport_state.scissorCount = 1;
   viewport_state.pScissors = &scissor;

   VkPipelineRasterizationStateCreateInfo rasterizer{};
   rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   rasterizer.lineWidth = 1.0f;
   rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterizer.depthBiasEnable = VK_FALSE;

   VkPipelineMultisampleStateCreateInfo multisampling{};
   multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   VkPipelineDepthStencilStateCreateInfo depth_stencil{};
   depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   depth_stencil.depthTestEnable = VK_TRUE;
   depth_stencil.depthWriteEnable = VK_TRUE;
   depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
   depth_stencil.depthBoundsTestEnable = VK_FALSE;
   depth_stencil.stencilTestEnable = VK_FALSE;

   VkPipelineColorBlendAttachmentState color_blend_attachment{};
   color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   color_blend_attachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo color_blending{};
   color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   color_blending.logicOpEnable = VK_FALSE;
   color_blending.logicOp = VK_LOGIC_OP_COPY;
   color_blending.attachmentCount = 1;
   color_blending.pAttachments = &color_blend_attachment;
   color_blending.blendConstants[0] = 0.0f;
   color_blending.blendConstants[1] = 0.0f;
   color_blending.blendConstants[2] = 0.0f;
   color_blending.blendConstants[3] = 0.0f;

   VkPipelineLayoutCreateInfo pipeline_layout_info{};
   pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipeline_layout_info.setLayoutCount = 1;
   pipeline_layout_info.pSetLayouts = &DescriptorSetLayout;

   VkResult result = vkCreatePipelineLayout(
      Device,
      &pipeline_layout_info,
      nullptr,
      &PipelineLayout
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create pipeline layout!");

   std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = { vert_shader_stage_info, frag_shader_stage_info };
   VkGraphicsPipelineCreateInfo pipeline_info{};
   pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipeline_info.stageCount = shader_stages.size();
   pipeline_info.pStages = shader_stages.data();
   pipeline_info.pVertexInputState = &vertex_input_info;
   pipeline_info.pInputAssemblyState = &input_assembly;
   pipeline_info.pViewportState = &viewport_state;
   pipeline_info.pRasterizationState = &rasterizer;
   pipeline_info.pMultisampleState = &multisampling;
   pipeline_info.pDepthStencilState = &depth_stencil;
   pipeline_info.pColorBlendState = &color_blending;
   pipeline_info.layout = PipelineLayout;
   pipeline_info.renderPass = RenderPass;
   pipeline_info.subpass = 0;
   pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

   result = vkCreateGraphicsPipelines(
      Device,
      VK_NULL_HANDLE,
      1,
      &pipeline_info,
      nullptr,
      &GraphicsPipeline
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create graphics pipeline!");

   vkDestroyShaderModule( Device, frag_shader_module, nullptr );
   vkDestroyShaderModule( Device, vert_shader_module, nullptr );
}

void RendererVK::createFramebuffers()
{
   SwapChainFramebuffers.resize( SwapChainImageViews.size() );
   for (size_t i = 0; i < SwapChainImageViews.size(); ++i) {
      std::array<VkImageView, 2> attachments = { SwapChainImageViews[i], DepthImageView };
      VkFramebufferCreateInfo framebuffer_info{};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = RenderPass;
      framebuffer_info.attachmentCount = attachments.size();
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = SwapChainExtent.width;
      framebuffer_info.height = SwapChainExtent.height;
      framebuffer_info.layers = 1;

      const VkResult result = vkCreateFramebuffer(
         Device,
         &framebuffer_info,
         nullptr,
         &SwapChainFramebuffers[i]
      );
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
   }
}

void RendererVK::createCommandPool()
{
   QueueFamilyIndices queue_family_indices = findQueueFamilies( PhysicalDevice );

   VkCommandPoolCreateInfo pool_info{};
   pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool_info.queueFamilyIndex = queue_family_indices.GraphicsFamily.value();

   const VkResult result = vkCreateCommandPool(
      Device,
      &pool_info,
      nullptr,
      &CommandPool
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create command pool!");
}

VkFormat RendererVK::findSupportedFormat(
   const std::vector<VkFormat>& candidates,
   VkImageTiling tiling,
   VkFormatFeatureFlags features
)
{
   for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties( PhysicalDevice, format, &props );

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
      if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
   }
   throw std::runtime_error("failed to find supported format!");
}

VkFormat RendererVK::findDepthFormat()
{
   return findSupportedFormat(
      { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
   );
}

bool RendererVK::hasStencilComponent(VkFormat format)
{
   return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void RendererVK::createImage(
   uint32_t width,
   uint32_t height,
   VkFormat format,
   VkImageTiling tiling,
   VkImageUsageFlags usage,
   VkMemoryPropertyFlags properties,
   VkImage& image,
   VkDeviceMemory& image_memory
)
{
   VkImageCreateInfo image_info{};
   image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image_info.imageType = VK_IMAGE_TYPE_2D;
   image_info.extent.width = width;
   image_info.extent.height = height;
   image_info.extent.depth = 1;
   image_info.mipLevels = 1;
   image_info.arrayLayers = 1;
   image_info.format = format;
   image_info.tiling = tiling;
   image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   image_info.usage = usage;
   image_info.samples = VK_SAMPLE_COUNT_1_BIT;
   image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VkResult result = vkCreateImage( Device, &image_info, nullptr, &image );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create image!");

   VkMemoryRequirements memory_requirements;
   vkGetImageMemoryRequirements( Device, image, &memory_requirements);

   VkMemoryAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate_info.allocationSize = memory_requirements.size;
   allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   result = vkAllocateMemory(
      Device,
      &allocate_info,
      nullptr,
      &image_memory
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate image memory!");

   vkBindImageMemory( Device, image, image_memory, 0 );
}

VkImageView RendererVK::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
   VkImageViewCreateInfo view_info{};
   view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view_info.image = image;
   view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format = format;
   view_info.subresourceRange.aspectMask = aspect_flags;
   view_info.subresourceRange.baseMipLevel = 0;
   view_info.subresourceRange.levelCount = 1;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.layerCount = 1;

   VkImageView image_view;
   const VkResult result = vkCreateImageView(
      Device,
      &view_info,
      nullptr,
      &image_view
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create texture image View!");
   return image_view;
}

void RendererVK::createDepthResources()
{
   VkFormat depth_format = findDepthFormat();
   createImage(
      SwapChainExtent.width, SwapChainExtent.height,
      depth_format,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DepthImage,
      DepthImageMemory
   );
   DepthImageView = createImageView(
      DepthImage,
      depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT
   );
}

VkCommandBuffer RendererVK::beginSingleTimeCommands()
{
   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandPool = CommandPool;
   allocate_info.commandBufferCount = 1;

   VkCommandBuffer command_buffer;
   vkAllocateCommandBuffers( Device, &allocate_info, &command_buffer );

   VkCommandBufferBeginInfo begin_info{};
   begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer( command_buffer, &begin_info );
   return command_buffer;
}

void RendererVK::endSingleTimeCommands(VkCommandBuffer command_buffer)
{
   vkEndCommandBuffer( command_buffer );

   VkSubmitInfo submit_info{};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers = &command_buffer;

   vkQueueSubmit( GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE );
   vkQueueWaitIdle( GraphicsQueue );

   vkFreeCommandBuffers(
      Device,
      CommandPool,
      1,
      &command_buffer
   );
}

void RendererVK::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
   VkCommandBuffer command_buffer = beginSingleTimeCommands();

   VkImageMemoryBarrier barrier{};
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.oldLayout = old_layout;
   barrier.newLayout = new_layout;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel = 0;
   barrier.subresourceRange.levelCount = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount = 1;

   VkPipelineStageFlags source_stage;
   VkPipelineStageFlags destination_stage;
   if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
   }
   else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
   }
   else throw std::invalid_argument("unsupported layout transition!");

   vkCmdPipelineBarrier(
      command_buffer,
      source_stage, destination_stage,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier
   );

   endSingleTimeCommands( command_buffer );
}

void RendererVK::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
   VkCommandBuffer command_buffer = beginSingleTimeCommands();

   VkBufferImageCopy region{};
   region.bufferOffset = 0;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount = 1;
   region.imageOffset = { 0, 0, 0 };
   region.imageExtent = { width, height, 1 };

   vkCmdCopyBufferToImage(
      command_buffer,
      buffer,
      image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
   );

   endSingleTimeCommands( command_buffer );
}

void RendererVK::createTextureImage()
{
   const std::string file_path = "../emoy.png";
   const FREE_IMAGE_FORMAT format = FreeImage_GetFileType( file_path.c_str(), 0 );
   FIBITMAP* texture = FreeImage_Load( format, file_path.c_str() );

   FIBITMAP* texture_converted;
   constexpr uint n_bits = 32;
   const uint n_bits_per_pixel = FreeImage_GetBPP( texture );
   texture_converted = n_bits_per_pixel == n_bits ? texture : FreeImage_ConvertTo32Bits( texture );

   const uint width = FreeImage_GetWidth( texture_converted );
   const uint height = FreeImage_GetHeight( texture_converted );
   void* pixels = FreeImage_GetBits( texture_converted );
   if (!pixels) throw std::runtime_error("failed to load texture image!");

   VkBuffer staging_buffer;
   VkDeviceMemory staging_buffer_memory;
   VkDeviceSize image_size = width * height * 4;
   createBuffer(
   image_size,
   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
   staging_buffer,
   staging_buffer_memory
   );

   void* data;
   vkMapMemory( Device, staging_buffer_memory, 0, image_size, 0, &data );
      memcpy( data, pixels, static_cast<size_t>(image_size) );
   vkUnmapMemory( Device, staging_buffer_memory );

   FreeImage_Unload( texture_converted );
   if (n_bits_per_pixel != n_bits) FreeImage_Unload( texture );

   createImage(
      width, height,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      TextureImage,
      TextureImageMemory
   );

   transitionImageLayout(
      TextureImage,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
   );
   copyBufferToImage(
      staging_buffer,
      TextureImage,
      static_cast<uint32_t>(width), static_cast<uint32_t>(height)
   );
   transitionImageLayout(
      TextureImage,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
   );

   vkDestroyBuffer( Device, staging_buffer, nullptr );
   vkFreeMemory( Device, staging_buffer_memory, nullptr );
}

void RendererVK::createTextureImageView()
{
   TextureImageView = createImageView(
      TextureImage,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT
   );
}

void RendererVK::createTextureSampler()
{
   VkPhysicalDeviceProperties properties{};
   vkGetPhysicalDeviceProperties( PhysicalDevice, &properties );

   VkSamplerCreateInfo sampler_info{};
   sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler_info.magFilter = VK_FILTER_LINEAR;
   sampler_info.minFilter = VK_FILTER_LINEAR;
   sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   sampler_info.anisotropyEnable = VK_FALSE;
   sampler_info.maxAnisotropy = 1;//properties.limits.maxSamplerAnisotropy;
   sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
   sampler_info.unnormalizedCoordinates = VK_FALSE;
   sampler_info.compareEnable = VK_FALSE;
   sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
   sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

   const VkResult result = vkCreateSampler(
      Device,
      &sampler_info,
      nullptr,
      &TextureSampler
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create texture sampler!");
}

uint32_t RendererVK::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
   VkPhysicalDeviceMemoryProperties memory_properties;
   vkGetPhysicalDeviceMemoryProperties( PhysicalDevice, &memory_properties );
   for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
      if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
          return i;
      }
   }
   throw std::runtime_error("failed to find suitable memory type!");
}

void RendererVK::createBuffer(
   VkDeviceSize size,
   VkBufferUsageFlags usage,
   VkMemoryPropertyFlags properties,
   VkBuffer& buffer,
   VkDeviceMemory& buffer_memory
)
{
   VkBufferCreateInfo buffer_info{};
   buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer_info.size = size;
   buffer_info.usage = usage;
   buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VkResult result = vkCreateBuffer( Device, &buffer_info, nullptr, &buffer );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create buffer!");

   VkMemoryRequirements memory_requirements;
   vkGetBufferMemoryRequirements( Device, buffer, &memory_requirements );

   VkMemoryAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate_info.allocationSize = memory_requirements.size;
   allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   result = vkAllocateMemory(
      Device,
      &allocate_info,
      nullptr,
      &buffer_memory
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate buffer memory!");

   vkBindBufferMemory( Device, buffer, buffer_memory, 0 );
}

void RendererVK::copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandPool = CommandPool;
   allocate_info.commandBufferCount = 1;

   VkCommandBuffer command_buffer;
   vkAllocateCommandBuffers( Device, &allocate_info, &command_buffer );

   VkCommandBufferBeginInfo begin_info{};
   begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer( command_buffer, &begin_info );
      VkBufferCopy copy_region{};
      copy_region.size = size;
      vkCmdCopyBuffer(
         command_buffer,
         src_buffer, dst_buffer,
         1, &copy_region
      );
   vkEndCommandBuffer( command_buffer );

   VkSubmitInfo submit_info{};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers = &command_buffer;

   vkQueueSubmit( GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE );
   vkQueueWaitIdle( GraphicsQueue );

   vkFreeCommandBuffers(
      Device,
      CommandPool,
      1,
      &command_buffer
   );
}

void RendererVK::createVertexBuffer()
{
   VkBuffer staging_buffer;
   VkDeviceMemory staging_buffer_memory;
   VkDeviceSize buffer_size = sizeof( Vertices[0] ) * Vertices.size();
   createBuffer(
   buffer_size,
   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
   staging_buffer,
   staging_buffer_memory
   );

   void* data;
   vkMapMemory( Device, staging_buffer_memory, 0, buffer_size, 0, &data );
      memcpy( data, Vertices.data(), static_cast<size_t>(buffer_size) );
   vkUnmapMemory( Device, staging_buffer_memory );

   createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      VertexBuffer,
      VertexBufferMemory
   );
   copyBuffer( staging_buffer, VertexBuffer, buffer_size );

   vkDestroyBuffer( Device, staging_buffer, nullptr );
   vkFreeMemory( Device, staging_buffer_memory, nullptr );
}

void RendererVK::createIndexBuffer()
{
   VkBuffer staging_buffer;
   VkDeviceMemory staging_buffer_memory;
   VkDeviceSize buffer_size = sizeof( Indices[0] ) * Indices.size();
   createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      staging_buffer,
      staging_buffer_memory
   );

   void* data;
   vkMapMemory( Device, staging_buffer_memory, 0, buffer_size, 0, &data );
      memcpy( data, Indices.data(), static_cast<size_t>(buffer_size) );
   vkUnmapMemory( Device, staging_buffer_memory );

   createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      IndexBuffer,
      IndexBufferMemory
   );
   copyBuffer( staging_buffer, IndexBuffer, buffer_size );

   vkDestroyBuffer( Device, staging_buffer, nullptr );
   vkFreeMemory( Device, staging_buffer_memory, nullptr );
}

void RendererVK::createUniformBuffers()
{
   VkDeviceSize buffer_size = sizeof(UniformBufferObject);
   UniformBuffers.resize( MAX_FRAMES_IN_FLIGHT );
   UniformBuffersMemory.resize( MAX_FRAMES_IN_FLIGHT );
   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      createBuffer(
         buffer_size,
         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         UniformBuffers[i],
         UniformBuffersMemory[i]
      );
   }
}

void RendererVK::createDescriptorPool()
{
   std::array<VkDescriptorPoolSize, 2> pool_sizes{};
   pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
   pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

   VkDescriptorPoolCreateInfo pool_info{};
   pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
   pool_info.pPoolSizes = pool_sizes.data();
   pool_info.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

   const VkResult result = vkCreateDescriptorPool(
      Device,
      &pool_info,
      nullptr,
      &DescriptorPool
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create descriptor pool!");
}

void RendererVK::createDescriptorSets()
{
   std::vector<VkDescriptorSetLayout> layouts( MAX_FRAMES_IN_FLIGHT, DescriptorSetLayout);
   VkDescriptorSetAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocate_info.descriptorPool = DescriptorPool;
   allocate_info.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
   allocate_info.pSetLayouts = layouts.data();

   DescriptorSets.resize( MAX_FRAMES_IN_FLIGHT );
   const VkResult result = vkAllocateDescriptorSets(
      Device,
      &allocate_info,
      DescriptorSets.data()
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate descriptor sets!");

   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = UniformBuffers[i];
      buffer_info.offset = 0;
      buffer_info.range = sizeof( UniformBufferObject );

      VkDescriptorImageInfo image_info{};
      image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_info.imageView = TextureImageView;
      image_info.sampler = TextureSampler;

      std::array<VkWriteDescriptorSet, 2> descriptor_writes{};
      descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes[0].dstSet = DescriptorSets[i];
      descriptor_writes[0].dstBinding = 0;
      descriptor_writes[0].dstArrayElement = 0;
      descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_writes[0].descriptorCount = 1;
      descriptor_writes[0].pBufferInfo = &buffer_info;

      descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes[1].dstSet = DescriptorSets[i];
      descriptor_writes[1].dstBinding = 1;
      descriptor_writes[1].dstArrayElement = 0;
      descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptor_writes[1].descriptorCount = 1;
      descriptor_writes[1].pImageInfo = &image_info;

      vkUpdateDescriptorSets(
         Device,
         static_cast<uint32_t>(descriptor_writes.size()),
         descriptor_writes.data(),
         0,
         nullptr
      );
   }
}

void RendererVK::createCommandBuffer()
{
   CommandBuffers.resize( MAX_FRAMES_IN_FLIGHT );

   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.commandPool = CommandPool;
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());

   const VkResult result = vkAllocateCommandBuffers(
      Device,
      &allocate_info,
      CommandBuffers.data()
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate command buffers!");
}

void RendererVK::createSyncObjects()
{
   ImageAvailableSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
   RenderFinishedSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
   InFlightFences.resize( MAX_FRAMES_IN_FLIGHT );

   VkSemaphoreCreateInfo semaphore_info{};
   semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fence_info{};
   fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      const VkResult image_available_semaphore_result = vkCreateSemaphore(
         Device,
         &semaphore_info,
         nullptr,
         &ImageAvailableSemaphores[i]
      );
      const VkResult render_finished_semaphore_result = vkCreateSemaphore(
         Device,
         &semaphore_info,
         nullptr,
         &RenderFinishedSemaphores[i]
      );
      const VkResult fence_result = vkCreateFence(
         Device,
         &fence_info,
         nullptr,
         &InFlightFences[i]
      );

      if (image_available_semaphore_result != VK_SUCCESS ||
          render_finished_semaphore_result != VK_SUCCESS ||
          fence_result != VK_SUCCESS) {
         throw std::runtime_error("failed to create synchronization objects for a frame!");
      }
   }
}

void RendererVK::initializeVulkan()
{
   createInstance();
#ifdef _DEBUG
   setupDebugMessenger();
#endif
   createSurface();
   pickPhysicalDevice();
   createLogicalDevice();
   createSwapChain();
   createImageViews();
   createRenderPass();
   createDescriptorSetLayout();
   createGraphicsPipeline();
   createCommandPool();
   createDepthResources();
   createFramebuffers();
   createTextureImage();
   createTextureImageView();
   createTextureSampler();
   createVertexBuffer();
   createIndexBuffer();
   createUniformBuffers();
   createDescriptorPool();
   createDescriptorSets();
   createCommandBuffer();
   createSyncObjects();
}

void RendererVK::recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
   VkCommandBufferBeginInfo begin_info{};
   begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

   if (vkBeginCommandBuffer( command_buffer, &begin_info ) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
   }

   VkRenderPassBeginInfo render_pass_info{};
   render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   render_pass_info.renderPass = RenderPass;
   render_pass_info.framebuffer = SwapChainFramebuffers[image_index];
   render_pass_info.renderArea.offset = { 0, 0 };
   render_pass_info.renderArea.extent = SwapChainExtent;

   std::array<VkClearValue, 2> clear_values{};
   clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
   clear_values[1].depthStencil = { 1.0f, 0 };
   render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
   render_pass_info.pClearValues = clear_values.data();

   vkCmdBeginRenderPass( command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );
      vkCmdBindPipeline( command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline );
      const std::array<VkBuffer, 1> vertex_buffers = { VertexBuffer };
      constexpr std::array<VkDeviceSize, 1> offsets = { 0 };
      vkCmdBindVertexBuffers(
         command_buffer, 0, 1,
         vertex_buffers.data(), offsets.data()
      );
      vkCmdBindIndexBuffer(
         command_buffer,
         IndexBuffer,
         0,
         VK_INDEX_TYPE_UINT16
      );
      vkCmdBindDescriptorSets(
         command_buffer,
         VK_PIPELINE_BIND_POINT_GRAPHICS,
         PipelineLayout,
         0, 1, &DescriptorSets[CurrentFrame],
         0, nullptr
      );
      vkCmdDrawIndexed(
         command_buffer, static_cast<uint32_t>(Indices.size()),
         1, 0, 0, 0
      );
      //vkCmdDraw(
      //   command_buffer, static_cast<uint32_t>(Vertices.size()),
      //   1, 0, 0
      //);
   vkCmdEndRenderPass( command_buffer );

   if (vkEndCommandBuffer( command_buffer ) != VK_SUCCESS) {
      throw std::runtime_error( "failed to record command buffer!");
   }
}

void RendererVK::cleanupSwapChain()
{
   vkDestroyImageView( Device, DepthImageView, nullptr );
   vkDestroyImage( Device, DepthImage, nullptr );
   vkFreeMemory( Device, DepthImageMemory, nullptr );
   for (auto framebuffer : SwapChainFramebuffers) {
      vkDestroyFramebuffer( Device, framebuffer, nullptr );
   }
   vkDestroyPipeline( Device, GraphicsPipeline, nullptr );
   vkDestroyPipelineLayout( Device, PipelineLayout, nullptr );
   vkDestroyRenderPass( Device, RenderPass, nullptr );
   for (auto imageView : SwapChainImageViews) {
      vkDestroyImageView( Device, imageView, nullptr );
   }
   vkDestroySwapchainKHR( Device, SwapChain, nullptr );
}

void RendererVK::recreateSwapChain()
{
   int width = 0, height = 0;
   glfwGetFramebufferSize( Window, &width, &height );
   while (width == 0 || height == 0) {
      glfwGetFramebufferSize( Window, &width, &height );
      glfwWaitEvents();
   }
   vkDeviceWaitIdle( Device );

   cleanupSwapChain();
   createSwapChain();
   createImageViews();
   createRenderPass();
   createGraphicsPipeline();
   createDepthResources();
   createFramebuffers();
}

void RendererVK::updateUniformBuffer(uint32_t current_image)
{
   static auto start_time = std::chrono::high_resolution_clock::now();
   auto current_time = std::chrono::high_resolution_clock::now();
   float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time).count();

   UniformBufferObject ubo{};
   ubo.Model = glm::rotate(
      glm::mat4(1.0f),
      time * glm::radians( 90.0f ),
      glm::vec3(0.0f, 0.0f, 1.0f)
   );
   ubo.View = glm::lookAt(
      glm::vec3(2.0f, 2.0f, 2.0f),
      glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
   );
   ubo.Projection = glm::perspective(
      glm::radians( 45.0f ),
      static_cast<float>(SwapChainExtent.width) / static_cast<float>(SwapChainExtent.height),
      0.1f,
      10.0f
   );

   // glm was originally designed for OpenGL, where the y-coordinate of the clip coordinates is inverted.
   // The easiest way to compensate for that is to flip the sign on the scaling factor of the y-axis in the projection
   // matrix. If you do not do this, then the image will be rendered upside down.
   ubo.Projection[1][1] *= -1;

   void* data;
   vkMapMemory( Device, UniformBuffersMemory[current_image], 0, sizeof(ubo), 0, &data );
      memcpy( data, &ubo, sizeof( ubo ) );
   vkUnmapMemory( Device, UniformBuffersMemory[current_image] );
}

void RendererVK::drawFrame()
{
   vkWaitForFences(
      Device,
      1,
      &InFlightFences[CurrentFrame],
      VK_TRUE,
      UINT64_MAX
   );

   uint32_t image_index;
   VkResult result = vkAcquireNextImageKHR(
      Device,
      SwapChain,
      UINT64_MAX,
      ImageAvailableSemaphores[CurrentFrame],
      VK_NULL_HANDLE,
      &image_index
   );
   if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
   }
   else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
   }

   updateUniformBuffer( CurrentFrame );

   vkResetFences( Device, 1, &InFlightFences[CurrentFrame] );
   vkResetCommandBuffer( CommandBuffers[CurrentFrame], 0 );
   recordCommandBuffer( CommandBuffers[CurrentFrame], image_index );

   VkSubmitInfo submit_info{};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   std::array<VkSemaphore, 1> wait_semaphores = { ImageAvailableSemaphores[CurrentFrame] };
   std::array<VkSemaphore, 1> signal_semaphores = { RenderFinishedSemaphores[CurrentFrame] };
   std::array<VkPipelineStageFlags, 1> wait_stages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
   submit_info.waitSemaphoreCount = wait_semaphores.size();
   submit_info.pWaitSemaphores = wait_semaphores.data();
   submit_info.pWaitDstStageMask = wait_stages.data();
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers = &CommandBuffers[CurrentFrame];
   submit_info.signalSemaphoreCount = signal_semaphores.size();
   submit_info.pSignalSemaphores = signal_semaphores.data();

   result = vkQueueSubmit(
      GraphicsQueue,
      1,
      &submit_info,
      InFlightFences[CurrentFrame]
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to submit draw command buffer!");

   VkPresentInfoKHR present_info{};
   present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   present_info.waitSemaphoreCount = signal_semaphores.size();
   present_info.pWaitSemaphores = signal_semaphores.data();

   std::array<VkSwapchainKHR, 1> swap_chains = { SwapChain };
   present_info.swapchainCount = swap_chains.size();
   present_info.pSwapchains = swap_chains.data();
   present_info.pImageIndices = &image_index;
   result = vkQueuePresentKHR( PresentQueue, &present_info );

   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || FramebufferResized) {
      FramebufferResized = false;
      recreateSwapChain();
   }
   else if (result != VK_SUCCESS) throw std::runtime_error("failed to present swap chain image!");

   CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RendererVK::mainLoop()
{
   while (!glfwWindowShouldClose( Window )) {
      glfwPollEvents();
      drawFrame();
   }
   vkDeviceWaitIdle( Device );
}

void RendererVK::cleanup()
{
   cleanupSwapChain();
   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroyBuffer( Device, UniformBuffers[i], nullptr );
      vkFreeMemory( Device, UniformBuffersMemory[i], nullptr );
   }
   vkDestroyDescriptorPool( Device, DescriptorPool, nullptr );
   vkDestroySampler( Device, TextureSampler, nullptr );
   vkDestroyImageView( Device, TextureImageView, nullptr );
   vkDestroyImage( Device, TextureImage, nullptr );
   vkFreeMemory( Device, TextureImageMemory, nullptr );
   vkDestroyDescriptorSetLayout( Device, DescriptorSetLayout, nullptr);
   vkDestroyBuffer( Device, IndexBuffer, nullptr );
   vkFreeMemory( Device, IndexBufferMemory, nullptr );
   vkDestroyBuffer( Device, VertexBuffer, nullptr );
   vkFreeMemory( Device, VertexBufferMemory, nullptr );
   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore( Device, RenderFinishedSemaphores[i], nullptr );
      vkDestroySemaphore( Device, ImageAvailableSemaphores[i], nullptr );
      vkDestroyFence( Device, InFlightFences[i], nullptr );
   }
   vkDestroyCommandPool( Device, CommandPool, nullptr );
   vkDestroyDevice( Device, nullptr );
#ifdef _DEBUG
   destroyDebugUtilsMessengerEXT( Instance, DebugMessenger, nullptr );
#endif
   vkDestroySurfaceKHR( Instance, Surface, nullptr );
   vkDestroyInstance( Instance, nullptr );
   glfwDestroyWindow( Window );
   glfwTerminate();
}

std::vector<const char*> RendererVK::getRequiredExtensions()
{
   uint32_t glfw_extension_count = 0;
   const char** glfw_extensions = glfwGetRequiredInstanceExtensions( &glfw_extension_count );
   std::vector<const char*> extensions( glfw_extensions, glfw_extensions + glfw_extension_count);
#ifdef _DEBUG
   extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
#endif
   return extensions;
}

void RendererVK::createInstance()
{
#ifdef _DEBUG
   if (!checkValidationLayerSupport()) throw std::runtime_error("validation layers requested, but not available!");
#endif

   VkApplicationInfo application_info{};
   application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application_info.pApplicationName = "Hello Triangle";
   application_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0);
   application_info.pEngineName = "No Engine";
   application_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0);
   application_info.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   create_info.pApplicationInfo = &application_info;

   std::vector<const char*> extensions = getRequiredExtensions();
   create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
   create_info.ppEnabledExtensionNames = extensions.data();

#ifdef NDEBUG
   create_info.enabledLayerCount = 0;
   create_info.pNext = nullptr;
#else
   create_info.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
   create_info.ppEnabledLayerNames = ValidationLayers.data();

   VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
   populateDebugMessengerCreateInfo( debug_create_info );
   create_info.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debug_create_info);
#endif

   if (vkCreateInstance( &create_info, nullptr, &Instance ) != VK_SUCCESS) {
      throw std::runtime_error("failed to create Instance!");
   }
}