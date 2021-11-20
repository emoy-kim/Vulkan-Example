#include "vulkan_swap_chain.h"

SwapChainVK::SwapChainVK(DeviceVK* device, VkExtent2D extent) :
   SwapChainImageFormat{}, SwapChainExtent{}, RenderPass{}, Device( device ), WindowExtent( extent ), SwapChain{},
   CurrentFrame( 0 )
{
   initialize();
}

SwapChainVK::SwapChainVK(DeviceVK* device, VkExtent2D extent, std::shared_ptr<SwapChainVK> previous) :
   SwapChainImageFormat{}, SwapChainExtent{}, RenderPass{}, Device( device ), WindowExtent( extent ), SwapChain{},
   CurrentFrame( 0 ), PreviousSwapChain( previous )
{
   initialize();
   previous.reset();
}


void SwapChainVK::initialize()
{
   createSwapChain();
   createImageViews();
   createRenderPass();
   createDepthResources();
   createFrameBuffers();
   createSyncObjects();
}

SwapChainVK::~SwapChainVK()
{
   for (auto& imageView : SwapChainImageViews) {
      vkDestroyImageView( Device->device(), imageView, nullptr );
   }
   SwapChainImageViews.clear();

   if (SwapChain != nullptr) {
      vkDestroySwapchainKHR( Device->device(), SwapChain, nullptr );
      SwapChain = nullptr;
   }

   for (size_t i = 0; i < DepthImages.size(); i++) {
      vkDestroyImageView( Device->device(), DepthImageViews[i], nullptr );
      vkDestroyImage( Device->device(), DepthImages[i], nullptr );
      vkFreeMemory( Device->device(), DepthImageMemories[i], nullptr );
   }

   for (auto& framebuffer : SwapChainFrameBuffers) {
      vkDestroyFramebuffer( Device->device(), framebuffer, nullptr );
   }

   vkDestroyRenderPass( Device->device(), RenderPass, nullptr );

   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore( Device->device(), RenderFinishedSemaphores[i], nullptr );
      vkDestroySemaphore( Device->device(), ImageAvailableSemaphores[i], nullptr );
      vkDestroyFence( Device->device(), InFlightFences[i], nullptr );
   }
}

VkResult SwapChainVK::acquireNextImage(uint32_t* image_index)
{
   vkWaitForFences(
      Device->device(),
      1,
      &InFlightFences[CurrentFrame],
      VK_TRUE,
      std::numeric_limits<uint64_t>::max()
   );
   VkResult result = vkAcquireNextImageKHR(
      Device->device(),
      SwapChain,
      std::numeric_limits<uint64_t>::max(),
      ImageAvailableSemaphores[CurrentFrame],  // must be a not signaled semaphore
      VK_NULL_HANDLE,
      image_index
   );
   return result;
}

VkResult SwapChainVK::submitCommandBuffers(const VkCommandBuffer* buffers, const uint32_t* image_index)
{
   if (ImagesInFlight[*image_index] != VK_NULL_HANDLE) {
      vkWaitForFences( Device->device(), 1, &ImagesInFlight[*image_index], VK_TRUE, UINT64_MAX );
   }
   ImagesInFlight[*image_index] = InFlightFences[CurrentFrame];

   VkSubmitInfo submit_info{};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   VkSemaphore wait_semaphores[] = { ImageAvailableSemaphores[CurrentFrame] };
   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
   submit_info.waitSemaphoreCount = 1;
   submit_info.pWaitSemaphores = wait_semaphores;
   submit_info.pWaitDstStageMask = waitStages;
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers = buffers;

   VkSemaphore signal_semaphores[] = { RenderFinishedSemaphores[CurrentFrame] };
   submit_info.signalSemaphoreCount = 1;
   submit_info.pSignalSemaphores = signal_semaphores;

   vkResetFences( Device->device(), 1, &InFlightFences[CurrentFrame] );
   if (vkQueueSubmit( Device->graphicsQueue(), 1, &submit_info, InFlightFences[CurrentFrame] ) != VK_SUCCESS) {
      std::cout << "failed to submit draw command buffer!\n";
   }

   VkPresentInfoKHR present_info{};
   present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   present_info.waitSemaphoreCount = 1;
   present_info.pWaitSemaphores = signal_semaphores;

   VkSwapchainKHR swapChains[] = { SwapChain };
   present_info.swapchainCount = 1;
   present_info.pSwapchains = swapChains;
   present_info.pImageIndices = image_index;

   VkResult result = vkQueuePresentKHR( Device->presentQueue(), &present_info );
   CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
   return result;
}

void SwapChainVK::createSwapChain()
{
   SwapChainSupportDetails swap_chain_support = Device->getSwapChainSupport();

   VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat( swap_chain_support.formats );
   VkPresentModeKHR present_mode = chooseSwapPresentMode( swap_chain_support.presentModes );
   VkExtent2D extent = chooseSwapExtent( swap_chain_support.capabilities );

   uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
   if (swap_chain_support.capabilities.maxImageCount > 0 &&
      image_count > swap_chain_support.capabilities.maxImageCount) {
      image_count = swap_chain_support.capabilities.maxImageCount;
   }

   VkSwapchainCreateInfoKHR create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   create_info.surface = Device->surface();
   create_info.minImageCount = image_count;
   create_info.imageFormat = surface_format.format;
   create_info.imageColorSpace = surface_format.colorSpace;
   create_info.imageExtent = extent;
   create_info.imageArrayLayers = 1;
   create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   QueueFamilyIndices indices = Device->findPhysicalQueueFamilies();
   uint32_t queue_family_indices[] = { indices.graphicsFamily, indices.presentFamily };

   if (indices.graphicsFamily != indices.presentFamily) {
      create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices;
   }
   else {
      create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      create_info.queueFamilyIndexCount = 0;      // optional
      create_info.pQueueFamilyIndices = nullptr;  // optional
   }

   create_info.preTransform = swap_chain_support.capabilities.currentTransform;
   create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   create_info.presentMode = present_mode;
   create_info.clipped = VK_TRUE;
   create_info.oldSwapchain = PreviousSwapChain == nullptr ? VK_NULL_HANDLE : PreviousSwapChain->SwapChain;

   if (vkCreateSwapchainKHR( Device->device(), &create_info, nullptr, &SwapChain ) != VK_SUCCESS) {
      std::cout << "failed to create swap chain!\n";
   }

   // we only specified a minimum number of images in the swap chain, so the implementation is
   // allowed to create a swap chain with more. That's why we'll first query the final number of
   // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
   // retrieve the handles.
   vkGetSwapchainImagesKHR( Device->device(), SwapChain, &image_count, nullptr );
   SwapChainImages.resize( image_count );
   vkGetSwapchainImagesKHR( Device->device(), SwapChain, &image_count, SwapChainImages.data() );

   SwapChainImageFormat = surface_format.format;
   SwapChainExtent = extent;
}

void SwapChainVK::createImageViews()
{
   SwapChainImageViews.resize( SwapChainImages.size() );
   for (size_t i = 0; i < SwapChainImages.size(); ++i) {
      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = SwapChainImages[i];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = SwapChainImageFormat;
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.baseMipLevel = 0;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.baseArrayLayer = 0;
      view_info.subresourceRange.layerCount = 1;

      if (vkCreateImageView( Device->device(), &view_info, nullptr, &SwapChainImageViews[i] ) != VK_SUCCESS) {
         std::cout << "failed to create texture image view!\n";
      }
   }
}

void SwapChainVK::createRenderPass()
{
   VkAttachmentDescription depth_attachment{};
   depth_attachment.format = findDepthFormat();
   depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
   depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkAttachmentReference depth_attachment_ref{};
   depth_attachment_ref.attachment = 1;
   depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkAttachmentDescription color_attachment = {};
   color_attachment.format = getSwapChainImageFormat();
   color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
   color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

   VkAttachmentReference color_attachment_ref = {};
   color_attachment_ref.attachment = 0;
   color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subpass = {};
   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments = &color_attachment_ref;
   subpass.pDepthStencilAttachment = &depth_attachment_ref;

   VkSubpassDependency dependency = {};
   dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
   dependency.srcAccessMask = 0;
   dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
   dependency.dstSubpass = 0;
   dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
   dependency.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

   VkRenderPassCreateInfo renderPassInfo{};
   std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
   renderPassInfo.pAttachments = attachments.data();
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subpass;
   renderPassInfo.dependencyCount = 1;
   renderPassInfo.pDependencies = &dependency;

   if (vkCreateRenderPass( Device->device(), &renderPassInfo, nullptr, &RenderPass ) != VK_SUCCESS) {
      std::cout << "failed to create render pass!\n";
   }
}

void SwapChainVK::createFrameBuffers()
{
   SwapChainFrameBuffers.resize( imageCount() );
   for (size_t i = 0; i < imageCount(); ++i) {
      std::array<VkImageView, 2> attachments = { SwapChainImageViews[i], DepthImageViews[i] };

      VkFramebufferCreateInfo framebufferInfo{};
      VkExtent2D swap_chain_extent = getSwapChainExtent();
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = RenderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = swap_chain_extent.width;
      framebufferInfo.height = swap_chain_extent.height;
      framebufferInfo.layers = 1;

      VkResult result = vkCreateFramebuffer(
         Device->device(),
         &framebufferInfo,
         nullptr,
         &SwapChainFrameBuffers[i]
      );
      if (result != VK_SUCCESS) {
         std::cout << "failed to create framebuffer!\n";
         return;
      }
  }
}

void SwapChainVK::createDepthResources()
{
   VkFormat depthFormat = findDepthFormat();
   VkExtent2D swapChainExtent = getSwapChainExtent();

   DepthImages.resize( imageCount() );
   DepthImageMemories.resize( imageCount() );
   DepthImageViews.resize( imageCount() );

   for (size_t i = 0; i < DepthImages.size(); ++i) {
      VkImageCreateInfo image_info{};
      image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      image_info.imageType = VK_IMAGE_TYPE_2D;
      image_info.extent.width = swapChainExtent.width;
      image_info.extent.height = swapChainExtent.height;
      image_info.extent.depth = 1;
      image_info.mipLevels = 1;
      image_info.arrayLayers = 1;
      image_info.format = depthFormat;
      image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
      image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      image_info.samples = VK_SAMPLE_COUNT_1_BIT;
      image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      image_info.flags = 0;

      Device->createImageWithInfo(
         image_info,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
         DepthImages[i],
         DepthImageMemories[i]
      );

      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = DepthImages[i];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = depthFormat;
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      view_info.subresourceRange.baseMipLevel = 0;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.baseArrayLayer = 0;
      view_info.subresourceRange.layerCount = 1;

      if (vkCreateImageView( Device->device(), &view_info, nullptr, &DepthImageViews[i] ) != VK_SUCCESS) {
         std::cout << "failed to create texture image view!\n";
      }
   }
}

void SwapChainVK::createSyncObjects()
{
   ImageAvailableSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
   RenderFinishedSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
   InFlightFences.resize( MAX_FRAMES_IN_FLIGHT );
   ImagesInFlight.resize( imageCount(), VK_NULL_HANDLE );

   VkSemaphoreCreateInfo semaphore_info{};
   semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fence_info = {};
   fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      if (vkCreateSemaphore( Device->device(), &semaphore_info, nullptr, &ImageAvailableSemaphores[i] ) != VK_SUCCESS ||
          vkCreateSemaphore( Device->device(), &semaphore_info, nullptr, &RenderFinishedSemaphores[i] ) != VK_SUCCESS ||
          vkCreateFence( Device->device(), &fence_info, nullptr, &InFlightFences[i] ) != VK_SUCCESS) {
         std::cout << "failed to create synchronization objects for a frame!\n";
      }
  }
}

VkSurfaceFormatKHR SwapChainVK::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
   for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat;
  }
  return availableFormats[0];
}

VkPresentModeKHR SwapChainVK::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
   for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
         std::cout << "Present mode: Mailbox\n";
         return availablePresentMode;
      }
   }

   //for (const auto &availablePresentMode : availablePresentModes) {
   //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
   //      std::cout << "Present mode: Immediate\n";
   //      return availablePresentMode;
   //   }
   //}

   std::cout << "Present mode: V-Sync\n";
   return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChainVK::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
   if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;
   else {
      VkExtent2D actualExtent = WindowExtent;
      actualExtent.width = std::max(
         capabilities.minImageExtent.width,
         std::min( capabilities.maxImageExtent.width, actualExtent.width )
      );
      actualExtent.height = std::max(
         capabilities.minImageExtent.height,
         std::min( capabilities.maxImageExtent.height, actualExtent.height )
      );
      return actualExtent;
  }
}

VkFormat SwapChainVK::findDepthFormat()
{
   return Device->findSupportedFormat(
      { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
   );
}