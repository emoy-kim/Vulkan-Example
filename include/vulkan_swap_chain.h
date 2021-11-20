#pragma once

#include "vulkan_device.h"

class SwapChainVK
{
public:
   static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

   SwapChainVK(DeviceVK* device, VkExtent2D extent);
   SwapChainVK(DeviceVK* device, VkExtent2D extent, std::shared_ptr<SwapChainVK> previous);
   ~SwapChainVK();

   SwapChainVK(const SwapChainVK&) = delete;
   SwapChainVK(const SwapChainVK&&) = delete;
   SwapChainVK& operator=(const SwapChainVK&) = delete;
   SwapChainVK& operator=(const SwapChainVK&&) = delete;

   VkFramebuffer getFrameBuffer(size_t index) { return SwapChainFrameBuffers[index]; }
   VkRenderPass getRenderPass() { return RenderPass; }
   VkImageView getImageView(size_t index) { return SwapChainImageViews[index]; }
   size_t imageCount() { return SwapChainImages.size(); }
   VkFormat getSwapChainImageFormat() { return SwapChainImageFormat; }
   VkExtent2D getSwapChainExtent() { return SwapChainExtent; }
   [[nodiscard]] uint32_t width() const { return SwapChainExtent.width; }
   [[nodiscard]] uint32_t height() const { return SwapChainExtent.height; }
   [[nodiscard]] float extentAspectRatio() const
   {
      return static_cast<float>(SwapChainExtent.width) / static_cast<float>(SwapChainExtent.height);
   }
   VkFormat findDepthFormat();
   VkResult acquireNextImage(uint32_t* image_index);
   VkResult submitCommandBuffers(const VkCommandBuffer* buffers, const uint32_t* image_index);

private:
   VkFormat SwapChainImageFormat;
   VkExtent2D SwapChainExtent;

   std::vector<VkFramebuffer> SwapChainFrameBuffers;
   VkRenderPass RenderPass;

   std::vector<VkImage> DepthImages;
   std::vector<VkDeviceMemory> DepthImageMemories;
   std::vector<VkImageView> DepthImageViews;
   std::vector<VkImage> SwapChainImages;
   std::vector<VkImageView> SwapChainImageViews;

   DeviceVK* Device;
   VkExtent2D WindowExtent;

   VkSwapchainKHR SwapChain;
   std::shared_ptr<SwapChainVK> PreviousSwapChain;

   std::vector<VkSemaphore> ImageAvailableSemaphores;
   std::vector<VkSemaphore> RenderFinishedSemaphores;
   std::vector<VkFence> InFlightFences;
   std::vector<VkFence> ImagesInFlight;
   size_t CurrentFrame;

   void initialize();
   void createSwapChain();
   void createImageViews();
   void createDepthResources();
   void createRenderPass();
   void createFrameBuffers();
   void createSyncObjects();

   static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
   static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
   VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};