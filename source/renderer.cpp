#include "renderer.h"

RendererVK::RendererVK() :
   FrameWidth( 1280 ), FrameHeight( 720 ), Common( std::make_shared<CommonVK>() ), Window( nullptr ), Instance{},
   Surface{}, SwapChain{}, SwapChainImageFormat{}, SwapChainExtent{}, DepthImage{}, DepthImageMemory{},
   DepthImageView{}, VertexBuffer{}, VertexBufferMemory{}, CurrentFrame( 0 ), FramebufferResized( false )
{
}

RendererVK::~RendererVK()
{
   VkDevice device = CommonVK::getDevice();
   cleanupSwapChain();
   vkDestroyBuffer( device, VertexBuffer, nullptr );
   vkFreeMemory( device, VertexBufferMemory, nullptr );
   for (size_t i = 0; i < CommonVK::getMaxFramesInFlight(); i++) {
      vkDestroySemaphore( device, RenderFinishedSemaphores[i], nullptr );
      vkDestroySemaphore( device, ImageAvailableSemaphores[i], nullptr );
      vkDestroyFence( device, InFlightFences[i], nullptr );
   }
   vkDestroyCommandPool( device, CommonVK::getCommandPool(), nullptr );
   vkDestroyDevice( device, nullptr );
#ifdef _DEBUG
   destroyDebugUtilsMessengerEXT( Instance, DebugMessenger, nullptr );
#endif
   vkDestroySurfaceKHR( Instance, Surface, nullptr );
   vkDestroyInstance( Instance, nullptr );
   glfwDestroyWindow( Window );
   glfwTerminate();
}

void RendererVK::cleanupSwapChain()
{
   UpperSquareObject.reset();
   LowerSquareObject.reset();
   Shader.reset();

   VkDevice device = CommonVK::getDevice();
   vkDestroyImageView( device, DepthImageView, nullptr );
   vkDestroyImage( device, DepthImage, nullptr );
   vkFreeMemory( device, DepthImageMemory, nullptr );
   for (auto framebuffer : SwapChainFramebuffers) {
      vkDestroyFramebuffer( device, framebuffer, nullptr );
   }
   for (auto image_view : SwapChainImageViews) {
      vkDestroyImageView( device, image_view, nullptr );
   }
   vkDestroySwapchainKHR( device, SwapChain, nullptr );
}

void RendererVK::initializeWindow()
{
   glfwInit();
   glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
   glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );
   Window = glfwCreateWindow(
      static_cast<int>(FrameWidth), static_cast<int>(FrameHeight),
      "Vulkan", nullptr, nullptr
   );
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
#endif

void RendererVK::createSurface()
{
   const VkResult result = glfwCreateWindowSurface(
      Instance,
      Window,
      nullptr,
      &Surface
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create Window Surface!");
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
   CommonVK::SwapChainSupportDetails swap_chain_support =
      CommonVK::querySwapChainSupport( CommonVK::getPhysicalDevice(), Surface );
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
   create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

   CommonVK::QueueFamilyIndices indices =
      CommonVK::findQueueFamilies( CommonVK::getPhysicalDevice(), Surface );
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
      CommonVK::getDevice(),
      &create_info,
      nullptr,
      &SwapChain
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create swap chain!");

   vkGetSwapchainImagesKHR(
      CommonVK::getDevice(),
      SwapChain,
      &image_count,
      nullptr
   );
   SwapChainImages.resize( image_count );
   vkGetSwapchainImagesKHR(
      CommonVK::getDevice(),
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
         CommonVK::getDevice(),
         &create_info,
         nullptr,
         &SwapChainImageViews[i]
      );
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create image views!");
   }
}

void RendererVK::createObject()
{
   UpperSquareObject = std::make_shared<ObjectVK>( Common.get() );
   UpperSquareObject->setSquareObject( std::filesystem::path(CMAKE_SOURCE_DIR) / "emoy.png" );
   UpperSquareObject->createDescriptorPool();
   UpperSquareObject->createUniformBuffers();
   UpperSquareObject->createDescriptorSets( Shader->getDescriptorSetLayout() );

   LowerSquareObject = std::make_shared<ObjectVK>( Common.get() );
   LowerSquareObject->setSquareObject( std::filesystem::path(CMAKE_SOURCE_DIR) / "emoy.png" );
   LowerSquareObject->createDescriptorPool();
   LowerSquareObject->createUniformBuffers();
   LowerSquareObject->createDescriptorSets( Shader->getDescriptorSetLayout() );
}

void RendererVK::createGraphicsPipeline()
{
   Shader = std::make_shared<ShaderVK>( Common.get() );
   Shader->createRenderPass( SwapChainImageFormat );
   Shader->createDescriptorSetLayout();
   Shader->createGraphicsPipeline(
      std::filesystem::path(CMAKE_SOURCE_DIR) / "shaders/shader.vert.spv",
      std::filesystem::path(CMAKE_SOURCE_DIR) / "shaders/shader.frag.spv",
      ObjectVK::getBindingDescription(),
      ObjectVK::getAttributeDescriptions(),
      { FrameWidth, FrameHeight }
   );
}

void RendererVK::createFramebuffers()
{
   SwapChainFramebuffers.resize( SwapChainImageViews.size() );
   for (size_t i = 0; i < SwapChainImageViews.size(); ++i) {
      std::array<VkImageView, 2> attachments = { SwapChainImageViews[i], DepthImageView };
      VkFramebufferCreateInfo framebuffer_info{};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = Shader->getRenderPass();
      framebuffer_info.attachmentCount = attachments.size();
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = SwapChainExtent.width;
      framebuffer_info.height = SwapChainExtent.height;
      framebuffer_info.layers = 1;

      const VkResult result = vkCreateFramebuffer(
         CommonVK::getDevice(),
         &framebuffer_info,
         nullptr,
         &SwapChainFramebuffers[i]
      );
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
   }
}

void RendererVK::createDepthResources()
{
   VkFormat depth_format = CommonVK::findDepthFormat();
   CommonVK::createImage(
      SwapChainExtent.width, SwapChainExtent.height,
      depth_format,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DepthImage,
      DepthImageMemory
   );
   DepthImageView = CommonVK::createImageView(
      DepthImage,
      depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT
   );
}

void RendererVK::copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandPool = CommonVK::getCommandPool();
   allocate_info.commandBufferCount = 1;

   VkCommandBuffer command_buffer;
   vkAllocateCommandBuffers( CommonVK::getDevice(), &allocate_info, &command_buffer );

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

   vkQueueSubmit( CommonVK::getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE );
   vkQueueWaitIdle( CommonVK::getGraphicsQueue() );

   vkFreeCommandBuffers(
      CommonVK::getDevice(),
      CommonVK::getCommandPool(),
      1,
      &command_buffer
   );
}

void RendererVK::createVertexBuffer()
{
   VkBuffer staging_buffer;
   VkDeviceMemory staging_buffer_memory;
   const VkDeviceSize buffer_size = LowerSquareObject->getVertexBufferSize();
   CommonVK::createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      staging_buffer,
      staging_buffer_memory
   );

   void* data;
   vkMapMemory(
      CommonVK::getDevice(),
      staging_buffer_memory,
      0, buffer_size, 0, &data
   );
      memcpy( data, LowerSquareObject->getVertexData(), static_cast<size_t>(buffer_size) );
   vkUnmapMemory( CommonVK::getDevice(), staging_buffer_memory );

   CommonVK::createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      VertexBuffer,
      VertexBufferMemory
   );
   copyBuffer( staging_buffer, VertexBuffer, buffer_size );

   vkDestroyBuffer( CommonVK::getDevice(), staging_buffer, nullptr );
   vkFreeMemory( CommonVK::getDevice(), staging_buffer_memory, nullptr );
}

void RendererVK::createCommandBuffer()
{
   CommandBuffers.resize( CommonVK::getMaxFramesInFlight() );

   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.commandPool = CommonVK::getCommandPool();
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());

   const VkResult result = vkAllocateCommandBuffers(
      CommonVK::getDevice(),
      &allocate_info,
      CommandBuffers.data()
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate command buffers!");
}

void RendererVK::createSyncObjects()
{
   const int max_frames_in_flight = CommonVK::getMaxFramesInFlight();
   ImageAvailableSemaphores.resize( max_frames_in_flight );
   RenderFinishedSemaphores.resize( max_frames_in_flight );
   InFlightFences.resize( max_frames_in_flight );

   VkSemaphoreCreateInfo semaphore_info{};
   semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fence_info{};
   fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   for (size_t i = 0; i < max_frames_in_flight; ++i) {
      const VkResult image_available_semaphore_result = vkCreateSemaphore(
         CommonVK::getDevice(),
         &semaphore_info,
         nullptr,
         &ImageAvailableSemaphores[i]
      );
      const VkResult render_finished_semaphore_result = vkCreateSemaphore(
         CommonVK::getDevice(),
         &semaphore_info,
         nullptr,
         &RenderFinishedSemaphores[i]
      );
      const VkResult fence_result = vkCreateFence(
         CommonVK::getDevice(),
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
   Common->pickPhysicalDevice( Instance, Surface );
   Common->createLogicalDevice( Surface );
   Common->createCommandPool( Surface );
   createSwapChain();
   createImageViews();
   createGraphicsPipeline();
   createObject();
   createDepthResources();
   createFramebuffers();
   createVertexBuffer();
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
   render_pass_info.renderPass = Shader->getRenderPass();
   render_pass_info.framebuffer = SwapChainFramebuffers[image_index];
   render_pass_info.renderArea.offset = { 0, 0 };
   render_pass_info.renderArea.extent = SwapChainExtent;

   std::array<VkClearValue, 2> clear_values{};
   clear_values[0].color = { { 0.35f, 0.0f, 0.53f, 1.0f } };
   clear_values[1].depthStencil = { 1.0f, 0 };
   render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
   render_pass_info.pClearValues = clear_values.data();

   vkCmdBeginRenderPass(
      command_buffer,
      &render_pass_info,
      VK_SUBPASS_CONTENTS_INLINE
   );
      vkCmdBindPipeline(
         command_buffer,
         VK_PIPELINE_BIND_POINT_GRAPHICS,
         Shader->getGraphicsPipeline()
      );
      const std::array<VkBuffer, 1> vertex_buffers = { VertexBuffer };
      constexpr std::array<VkDeviceSize, 1> offsets = { 0 };
      vkCmdBindVertexBuffers(
         command_buffer, 0, 1,
         vertex_buffers.data(), offsets.data()
      );

      vkCmdBindDescriptorSets(
         command_buffer,
         VK_PIPELINE_BIND_POINT_GRAPHICS,
         Shader->getPipelineLayout(),
         0, 1,
         LowerSquareObject->getDescriptorSet( CurrentFrame ),
         0, nullptr
      );
      vkCmdDraw(
         command_buffer, LowerSquareObject->getVertexSize(),
         1, 0, 0
      );

      vkCmdBindDescriptorSets(
         command_buffer,
         VK_PIPELINE_BIND_POINT_GRAPHICS,
         Shader->getPipelineLayout(),
         0, 1,
         UpperSquareObject->getDescriptorSet( CurrentFrame ),
         0, nullptr
      );
      vkCmdDraw(
         command_buffer, UpperSquareObject->getVertexSize(),
         1, 0, 0
      );
   vkCmdEndRenderPass( command_buffer );

   if (vkEndCommandBuffer( command_buffer ) != VK_SUCCESS) {
      throw std::runtime_error( "failed to record command buffer!");
   }
}

void RendererVK::recreateSwapChain()
{
   int width = 0, height = 0;
   glfwGetFramebufferSize( Window, &width, &height );
   while (width == 0 || height == 0) {
      glfwGetFramebufferSize( Window, &width, &height );
      glfwWaitEvents();
   }
   vkDeviceWaitIdle( CommonVK::getDevice() );

   cleanupSwapChain();
   createSwapChain();
   createImageViews();
   createGraphicsPipeline();
   createDepthResources();
   createFramebuffers();
}

void RendererVK::drawFrame()
{
   vkWaitForFences(
      CommonVK::getDevice(),
      1,
      &InFlightFences[CurrentFrame],
      VK_TRUE,
      UINT64_MAX
   );

   uint32_t image_index;
   VkResult result = vkAcquireNextImageKHR(
      CommonVK::getDevice(),
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

   static auto start_time = std::chrono::high_resolution_clock::now();
   auto current_time = std::chrono::high_resolution_clock::now();
   float time = std::chrono::duration<float, std::chrono::seconds::period>( current_time - start_time).count();
   const glm::mat4 lower_world =
      glm::translate( glm::mat4(1.0f), glm::vec3(-0.25f, 0.0f, 0.0f) ) *
      glm::rotate(
         glm::mat4(1.0f),
         time * glm::radians( 90.0f ),
         glm::vec3(0.0f, 1.0f, 0.0f)
      ) * glm::translate( glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, 0.0f) );
   const glm::mat4 upper_world =
      glm::translate( glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f) ) * lower_world;
   LowerSquareObject->updateUniformBuffer( CurrentFrame, SwapChainExtent, lower_world );
   UpperSquareObject->updateUniformBuffer( CurrentFrame, SwapChainExtent, upper_world );

   vkResetFences( CommonVK::getDevice(), 1, &InFlightFences[CurrentFrame] );
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
      CommonVK::getGraphicsQueue(),
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
   result = vkQueuePresentKHR( CommonVK::getPresentQueue(), &present_info );

   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || FramebufferResized) {
      FramebufferResized = false;
      recreateSwapChain();
   }
   else if (result != VK_SUCCESS) throw std::runtime_error("failed to present swap chain image!");

   CurrentFrame = (CurrentFrame + 1) % CommonVK::getMaxFramesInFlight();
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
   if (!CommonVK::checkValidationLayerSupport()) {
      throw std::runtime_error("validation layers requested, but not available!");
   }
#endif

   VkApplicationInfo application_info{};
   application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application_info.pApplicationName = "Hello Vulkan";
   application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   application_info.pEngineName = "No Engine";
   application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
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
   create_info.enabledLayerCount = CommonVK::getValidationLayerSize();
   create_info.ppEnabledLayerNames = CommonVK::getValidationLayerNames();

   VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
   populateDebugMessengerCreateInfo( debug_create_info );
   create_info.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debug_create_info);
#endif

   if (vkCreateInstance( &create_info, nullptr, &Instance ) != VK_SUCCESS) {
      throw std::runtime_error("failed to create Instance!");
   }
}

void RendererVK::writeFrame()
{
   VkImage dst_image;
   VkDeviceMemory dst_image_memory;
   CommonVK::createImage(
      FrameWidth, FrameHeight,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_TILING_LINEAR,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      dst_image,
      dst_image_memory
   );

   VkCommandBuffer copy_command = CommonVK::createCommandBuffer( VK_COMMAND_BUFFER_LEVEL_PRIMARY );

   CommonVK::insertImageMemoryBarrier(
      copy_command,
      dst_image,
      0,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
   );

   CommonVK::insertImageMemoryBarrier(
      copy_command,
      SwapChainImages[CurrentFrame],
      VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
   );

   VkImageCopy image_copy_region{};
   image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   image_copy_region.srcSubresource.layerCount = 1;
   image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   image_copy_region.dstSubresource.layerCount = 1;
   image_copy_region.extent.width = FrameWidth;
   image_copy_region.extent.height = FrameHeight;
   image_copy_region.extent.depth = 1;

   vkCmdCopyImage(
      copy_command,
      SwapChainImages[CurrentFrame],
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &image_copy_region
   );

   CommonVK::insertImageMemoryBarrier(
      copy_command,
      dst_image,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
   );

   CommonVK::insertImageMemoryBarrier(
      copy_command,
      SwapChainImages[CurrentFrame],
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
   );

   CommonVK::flushCommandBuffer( copy_command );

   VkImageSubresource subresource{};
   subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   VkSubresourceLayout subresource_layout;
   vkGetImageSubresourceLayout(
      CommonVK::getDevice(),
      dst_image,
      &subresource,
      &subresource_layout
   );

   uint8_t* image_data;
   vkMapMemory( CommonVK::getDevice(), dst_image_memory, 0, VK_WHOLE_SIZE, 0, (void**)&image_data);
   image_data += subresource_layout.offset;

   const std::string file_name = std::filesystem::path(CMAKE_SOURCE_DIR) / "frame.png";
   FIBITMAP* image = FreeImage_ConvertFromRawBits(
      image_data,
      static_cast<int>(FrameWidth),
      static_cast<int>(FrameHeight),
      static_cast<int>(FrameWidth) * 4,
      32,
      FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, true
   );
   FreeImage_Save( FIF_PNG, image, file_name.c_str() );
   FreeImage_Unload( image );

   vkUnmapMemory( CommonVK::getDevice(), dst_image_memory );
   vkFreeMemory( CommonVK::getDevice(), dst_image_memory, nullptr );
   vkDestroyImage( CommonVK::getDevice(), dst_image, nullptr );
}

void RendererVK::play()
{
   initializeWindow();
   initializeVulkan();

   while (!glfwWindowShouldClose( Window )) {
      glfwPollEvents();
      drawFrame();
   }
   writeFrame();
   vkDeviceWaitIdle( CommonVK::getDevice() );
}