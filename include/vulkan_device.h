#pragma once

#include "base.h"

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
  uint32_t graphicsFamily;
  uint32_t presentFamily;
  bool graphicsFamilyHasValue = false;
  bool presentFamilyHasValue = false;
  [[nodiscard]] bool isComplete() const { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class DeviceVK
{
 public:
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  explicit DeviceVK(GLFWwindow* window);
  ~DeviceVK();

  // Not copyable or movable
  DeviceVK(const DeviceVK &) = delete;
  DeviceVK operator=(const DeviceVK &) = delete;
  DeviceVK(DeviceVK &&) = delete;
  DeviceVK &operator=(DeviceVK &&) = delete;

  VkCommandPool getCommandPool() { return CommandPool; }
  VkDevice device() { return Device; }
  VkSurfaceKHR surface() { return Surface; }
  VkQueue graphicsQueue() { return GraphicsQueue; }
  VkQueue presentQueue() { return PresentQueue; }

  SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport( PhysicalDevice); }
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies( PhysicalDevice); }
  VkFormat findSupportedFormat(
      const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

  // Buffer Helper Functions
  void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer &buffer,
      VkDeviceMemory &bufferMemory);
  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void copyBufferToImage(
      VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

  void createImageWithInfo(
      const VkImageCreateInfo &imageInfo,
      VkMemoryPropertyFlags properties,
      VkImage &image,
      VkDeviceMemory &imageMemory);

  VkPhysicalDeviceProperties properties;

 private:
  void createInstance();
  void setupDebugMessenger();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createCommandPool();

  // helper functions
  bool isDeviceSuitable(VkPhysicalDevice device);
  std::vector<const char *> getRequiredExtensions();
  bool checkValidationLayerSupport();
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
  void hasGLFWRequiredInstanceExtensions();
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkInstance Instance;
  VkDebugUtilsMessengerEXT DebugMessenger;
  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkCommandPool CommandPool;

  VkDevice Device;
  VkSurfaceKHR Surface;
  VkQueue GraphicsQueue;
  VkQueue PresentQueue;

  const std::vector<const char *> ValidationLayers = { "VK_LAYER_KHRONOS_validation"};
  const std::vector<const char *> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};