#pragma once

#include "base.h"

class CommonVK final
{
public:
   struct QueueFamilyIndices
   {
      std::optional<uint32_t> GraphicsFamily;
      std::optional<uint32_t> PresentFamily;

      [[nodiscard]] bool isComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
   };

   struct SwapChainSupportDetails
   {
      VkSurfaceCapabilitiesKHR Capabilities;
      std::vector<VkSurfaceFormatKHR> Formats;
      std::vector<VkPresentModeKHR> PresentModes;
   };

   CommonVK() = default;
   ~CommonVK() = default;

   [[nodiscard]] static uint32_t getValidationLayerSize() { return static_cast<uint32_t>(ValidationLayers.size()); }
   [[nodiscard]] static const char* const* getValidationLayerNames() { return ValidationLayers.data(); }
   [[nodiscard]] static int getMaxFramesInFlight() { return MaxFramesInFlight; }
   [[nodiscard]] static VkPhysicalDevice getPhysicalDevice() { return PhysicalDevice; }
   [[nodiscard]] static VkDevice getDevice() { return Device; }
   [[nodiscard]] static VkQueue getGraphicsQueue() { return GraphicsQueue; }
   [[nodiscard]] static VkQueue getPresentQueue() { return PresentQueue; }
   [[nodiscard]] static VkCommandPool getCommandPool() { return CommandPool; }
   [[nodiscard]] static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
   [[nodiscard]] static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
   [[nodiscard]] static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
   [[nodiscard]] static bool hasStencilComponent(VkFormat format)
   {
      return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
   }
   [[nodiscard]] static VkFormat findSupportedFormat(
      const std::vector<VkFormat>& candidates,
      VkImageTiling tiling,
      VkFormatFeatureFlags features
   );
   [[nodiscard]] static VkFormat findDepthFormat();
   [[nodiscard]] static uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
   static bool checkValidationLayerSupport();
   static void pickPhysicalDevice(VkInstance Instance, VkSurfaceKHR surface);
   static void createLogicalDevice(VkSurfaceKHR surface);
   static void createCommandPool(VkSurfaceKHR surface);
   static void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer& buffer,
      VkDeviceMemory& buffer_memory
   );
   static void createImage(
      uint32_t width,
      uint32_t height,
      VkFormat format,
      VkImageTiling tiling,
      VkImageUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkImage& image,
      VkDeviceMemory& image_memory
   );
   static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
   static VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level);
   static void flushCommandBuffer(VkCommandBuffer commandBuffer);
   static void insertImageMemoryBarrier(
      VkCommandBuffer command_buffer,
      VkImage image,
      VkAccessFlags src_access_mask,
      VkAccessFlags dst_access_mask,
      VkImageLayout old_image_layout,
      VkImageLayout new_image_layout,
      VkPipelineStageFlags src_stage_mask,
      VkPipelineStageFlags dst_stage_mask,
      VkImageSubresourceRange subresource_range
   );

private:
   inline static const std::array<const char*, 1> ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"
   };
   inline static const std::array<const char*, 1> DeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
   };
   inline static int MaxFramesInFlight = 2;
   inline static VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
   inline static VkDevice Device{};
   inline static VkQueue GraphicsQueue{};
   inline static VkQueue PresentQueue{};
   inline static VkCommandPool CommandPool{};

   static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
};