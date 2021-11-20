#include "renderer.h"

RendererVK::RendererVK() :
   Resized( false ), FrameWidth( 1024 ), FrameHeight( 1024 ), Window( nullptr ), PipelineLayout{}
{
}

RendererVK::~RendererVK()
{
   vkDestroyPipelineLayout( Device->device(), PipelineLayout, nullptr );
   GameObjects.clear();
   Pipeline.reset();
   SwapChain.reset();
   Device.reset();
   glfwDestroyWindow( Window );
   glfwTerminate();
}

#if 0
void RendererVK::printVulkanInformation() const
{
   std::cout << "****************************************************************\n";
   VkPhysicalDeviceProperties deviceProperties;
   vkGetPhysicalDeviceProperties( PhysicalDevice, &deviceProperties );
   std::cout << " - Vulkan Device name: " << deviceProperties.deviceName << "\n";
   std::cout << " - Vulkan Device type: " << tools::physicalDeviceTypeString( deviceProperties.deviceType ) << "\n";
   std::cout << " - Vulkan api version: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
   std::cout << "****************************************************************\n\n";
}

void RendererVK::copyToHost()
{
   const char* imagedata;

   // Create the linear tiled destination image to copy to and to read the memory from
   VkImageCreateInfo imgCreateInfo(initializers::imageCreateInfo());
   imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
   imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
   imgCreateInfo.extent.width = FrameWidth;
   imgCreateInfo.extent.height = FrameHeight;
   imgCreateInfo.extent.depth = 1;
   imgCreateInfo.arrayLayers = 1;
   imgCreateInfo.mipLevels = 1;
   imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
   imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
   imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   // Create the image
   VkImage dstImage;
   VK_CHECK_RESULT(vkCreateImage(Device, &imgCreateInfo, nullptr, &dstImage));
   // Create memory to back up the image
   VkMemoryRequirements memRequirements;
   VkMemoryAllocateInfo memAllocInfo(initializers::memoryAllocateInfo());
   VkDeviceMemory dstImageMemory;
   vkGetImageMemoryRequirements(Device, dstImage, &memRequirements);
   memAllocInfo.allocationSize = memRequirements.size;
   // Memory must be host visible to copy from
   memAllocInfo.memoryTypeIndex = getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   VK_CHECK_RESULT(vkAllocateMemory(Device, &memAllocInfo, nullptr, &dstImageMemory));
   VK_CHECK_RESULT(vkBindImageMemory(Device, dstImage, dstImageMemory, 0));

   // Do the actual blit from the offscreen image to our host visible destination image
   VkCommandBufferAllocateInfo cmdBufAllocateInfo = initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
   VkCommandBuffer copyCmd;
   VK_CHECK_RESULT(vkAllocateCommandBuffers(Device, &cmdBufAllocateInfo, &copyCmd));
   VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();
   VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

   // Transition destination image to transfer destination layout
   tools::insertImageMemoryBarrier(
      copyCmd,
      dstImage,
      0,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
   );

   // colorAttachment.image is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

   VkImageCopy imageCopyRegion{};
   imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   imageCopyRegion.srcSubresource.layerCount = 1;
   imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   imageCopyRegion.dstSubresource.layerCount = 1;
   imageCopyRegion.extent.width = FrameWidth;
   imageCopyRegion.extent.height = FrameHeight;
   imageCopyRegion.extent.depth = 1;

   vkCmdCopyImage(
      copyCmd,
      color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &imageCopyRegion
   );

   // Transition destination image to general layout, which is the required layout for mapping the image memory later on
   tools::insertImageMemoryBarrier(
      copyCmd,
      dstImage,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

   VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

   submitWork(copyCmd, queue);

   // Get layout of the image (including row pitch)
   VkImageSubresource subResource{};
   subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   VkSubresourceLayout subResourceLayout;

   vkGetImageSubresourceLayout(Device, dstImage, &subResource, &subResourceLayout);

   // Map image memory so we can start copying from it
   vkMapMemory(Device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imagedata);
   imagedata += subResourceLayout.offset;

/*
   Save host visible framebuffer image to disk (ppm format)
*/

   const char* filename = "../headless.ppm";

   std::ofstream file(filename, std::ios::out | std::ios::binary);

   // ppm header
   file << "P6\n" << FrameWidth << "\n" << FrameHeight << "\n" << 255 << "\n";

   // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
   // Check if source is BGR and needs swizzle
   std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
   const bool colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), VK_FORMAT_R8G8B8A8_UNORM) != formatsBGR.end());

   // ppm binary pixel data
   for (int32_t y = 0; y < FrameHeight; y++) {
      auto* row = (unsigned int*)imagedata;
      for (int32_t x = 0; x < FrameWidth; x++) {
         if (colorSwizzle) {
            file.write((char*)row + 2, 1);
            file.write((char*)row + 1, 1);
            file.write((char*)row, 1);
         }
         else {
            file.write((char*)row, 3);
         }
         row++;
      }
      imagedata += subResourceLayout.rowPitch;
   }
   file.close();

   // Clean up resources
   vkUnmapMemory(Device, dstImageMemory);
   vkFreeMemory(Device, dstImageMemory, nullptr);
   vkDestroyImage(Device, dstImage, nullptr);
}
#endif

void RendererVK::resizeCallback(GLFWwindow* window, int width, int height)
{
   auto renderer = reinterpret_cast<RendererVK*>(glfwGetWindowUserPointer( window ));
   renderer->Resized = true;
   renderer->FrameWidth = width;
   renderer->FrameHeight = height;
}

void RendererVK::loadGameObjects()
{
   std::vector<ModelVK::Vertex> vertices = {
      { ModelVK::Vertex{ 0.0f, -0.5f, 1.0f, 0.0f, 0.0f } },
      { ModelVK::Vertex{ 0.5f, 0.5f, 0.0f, 1.0f, 0.0f } },
      { ModelVK::Vertex{ -0.5f, 0.5f, 0.0f, 0.0f, 1.0f } }
   };
   auto model = std::make_shared<ModelVK>(Device.get(), vertices);
   auto triangle = GameObject::createGameObject();
   triangle.Model = model;
   triangle.Color = { 0.1f, 0.8f, 0.1f };
   triangle.Transform.Translation.x = 0.2f;
   triangle.Transform.Scale = { 2.0f, 0.5f };
   triangle.Transform.Rotation = 0.25f * glm::two_pi<float>();
   GameObjects.emplace_back( std::move( triangle ) );
}

void RendererVK::createPipelineLayout()
{
   VkPushConstantRange push_constant_range{};
   push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
   push_constant_range.offset = 0;
   push_constant_range.size = sizeof( SimplePushConstantData );

   VkPipelineLayoutCreateInfo pipeline_layout_info{};
   pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipeline_layout_info.setLayoutCount = 0;
   pipeline_layout_info.pSetLayouts = nullptr;
   pipeline_layout_info.pushConstantRangeCount = 1;
   pipeline_layout_info.pPushConstantRanges = &push_constant_range;
   if (vkCreatePipelineLayout( Device->device(), &pipeline_layout_info, nullptr, &PipelineLayout ) != VK_SUCCESS) {
      std::cout << "Could not create pipeline layout\n";
   }
}

void RendererVK::createPipeline()
{
   PipelineConfiguration pipeline_configuration{};
   PipelineVK::getDefaultPipelineConfiguration( pipeline_configuration );
   pipeline_configuration.RenderPass = SwapChain->getRenderPass();
   pipeline_configuration.PipelineLayout = PipelineLayout;
   Pipeline = std::make_shared<PipelineVK>(
      Device.get(),
      "../shaders/shader.vert.spv",
      "../shaders/shader.frag.spv",
      pipeline_configuration
   );
}

void RendererVK::recreateSwapChain()
{
   VkExtent2D extent = { static_cast<uint32_t>(FrameWidth), static_cast<uint32_t>(FrameHeight) };
   while (extent.width == 0 || extent.height == 0) {
      extent = { static_cast<uint32_t>(FrameWidth), static_cast<uint32_t>(FrameHeight) };
      glfwWaitEvents();
   }
   vkDeviceWaitIdle( Device->device() );

   if (SwapChain == nullptr) SwapChain = std::make_shared<SwapChainVK>(Device.get(), extent);
   else {
      SwapChain = std::make_shared<SwapChainVK>(Device.get(), extent, std::move( SwapChain ));
      if (SwapChain->imageCount() != CommandBuffers.size()) {
         freeCommandBuffers();
         createCommandBuffers();
      }
   }
   createPipeline();
}

void RendererVK::createCommandBuffers()
{
   CommandBuffers.resize( SwapChain->imageCount() );

   VkCommandBufferAllocateInfo allocation_info{};
   allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocation_info.commandPool = Device->getCommandPool();
   allocation_info.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());

   if (vkAllocateCommandBuffers( Device->device(), &allocation_info, CommandBuffers.data() ) != VK_SUCCESS) {
      std::cout << "Could not allocate command buffers\n";
      return;
   }
}

void RendererVK::freeCommandBuffers()
{
   vkFreeCommandBuffers(
      Device->device(),
      Device->getCommandPool(),
      static_cast<uint32_t>(CommandBuffers.size()),
      CommandBuffers.data()
   );
   CommandBuffers.clear();
}

void RendererVK::recordCommandBuffer(uint32_t image_index)
{
   VkCommandBufferBeginInfo begin_info{};
   begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   if (vkBeginCommandBuffer( CommandBuffers[image_index], &begin_info ) != VK_SUCCESS) {
      std::cout << "Could not begin recording command buffer\n";
      return;
   }
   VkRenderPassBeginInfo render_pass_begin_info{};
   render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   render_pass_begin_info.renderPass = SwapChain->getRenderPass();
   render_pass_begin_info.framebuffer = SwapChain->getFrameBuffer( image_index );
   render_pass_begin_info.renderArea.offset = { 0, 0 };
   render_pass_begin_info.renderArea.extent = SwapChain->getSwapChainExtent();

   std::array<VkClearValue, 2> clear_values{};
   clear_values[0].color = { 0.01f, 0.01f, 0.01f, 1.0f };
   // clear_values[0].depthStencil = ? this value would be ignored
   clear_values[1].depthStencil = { 1.0f, 0 };
   render_pass_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
   render_pass_begin_info.pClearValues = clear_values.data();

   vkCmdBeginRenderPass( CommandBuffers[image_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );

   VkViewport viewport{};
   viewport.x = 0.0f;
   viewport.y = 0.0f;
   viewport.width = static_cast<float>(SwapChain->getSwapChainExtent().width);
   viewport.height = static_cast<float>(SwapChain->getSwapChainExtent().height);
   viewport.minDepth = 0.0f;
   viewport.maxDepth = 1.0f;
   VkRect2D scissor{ { 0, 0 }, SwapChain->getSwapChainExtent() };
   vkCmdSetViewport( CommandBuffers[image_index], 0, 1, &viewport );
   vkCmdSetScissor( CommandBuffers[image_index], 0, 1, &scissor );

   renderGameObjects( CommandBuffers[image_index] );

   vkCmdEndRenderPass( CommandBuffers[image_index] );
   if (vkEndCommandBuffer( CommandBuffers[image_index] ) != VK_SUCCESS) {
      std::cout << "Could not record command buffer\n";
      return;
   }
}

void RendererVK::initializeVulkan()
{
   glfwInit();
   glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
   glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );
   Window = glfwCreateWindow( FrameWidth, FrameHeight, "Vulkan Example", nullptr, nullptr );
   glfwSetWindowUserPointer( Window, this );
   glfwSetFramebufferSizeCallback( Window, resizeCallback );

   Device = std::make_shared<DeviceVK>(Window);
   SwapChain = std::make_shared<SwapChainVK>(
      Device.get(),
      VkExtent2D{ static_cast<uint32_t>(FrameWidth), static_cast<uint32_t>(FrameHeight) }
   );

   loadGameObjects();
   createPipelineLayout();
   recreateSwapChain();
   createCommandBuffers();
}

void RendererVK::renderGameObjects(VkCommandBuffer command_buffer)
{
   Pipeline->bind( command_buffer );

   for (auto& object : GameObjects) {
      object.Transform.Rotation = glm::mod( object.Transform.Rotation + 0.01f, glm::two_pi<float>() );

      SimplePushConstantData push{};
      push.Offset = object.Transform.Translation;
      push.Color = object.Color;
      push.Transform = object.Transform.mat2();
      vkCmdPushConstants(
         command_buffer,
         PipelineLayout,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
         0,
         sizeof( SimplePushConstantData ),
         &push
      );
      object.Model->bind( command_buffer );
      object.Model->draw( command_buffer );
   }
}

void RendererVK::render()
{
   uint32_t image_index;
   VkResult result = SwapChain->acquireNextImage( &image_index );

   if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
   }

   if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      std::cout << "Could not acquire swap chain image\n";
      return;
   }

   recordCommandBuffer( image_index );
   result = SwapChain->submitCommandBuffers( &CommandBuffers[image_index], &image_index );
   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Resized) {
      Resized = false;
      recreateSwapChain();
   }
   if (result != VK_SUCCESS) {
      std::cout << "Could not present swap chain image\n";
      return;
   }
}

void RendererVK::play()
{
   if (Window == nullptr) initializeVulkan();

   while (!glfwWindowShouldClose( Window )) {
      glfwPollEvents();
      render();
   }

   vkDeviceWaitIdle( Device->device() );
}