#pragma once

#include "base.h"

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices
{
   uint32_t GraphicsFamily;
   uint32_t PresentFamily;
   bool GraphicsFamilyHasValue;
   bool PresentFamilyHasValue;

   QueueFamilyIndices() :
      GraphicsFamily( 0 ), PresentFamily( 0 ), GraphicsFamilyHasValue( false ), PresentFamilyHasValue( false ) {}

   [[nodiscard]] bool isComplete() const { return GraphicsFamilyHasValue && PresentFamilyHasValue; }
};

class DeviceVK
{
public:
   VkPhysicalDeviceProperties Properties;

   explicit DeviceVK(GLFWwindow* window);
   ~DeviceVK();

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
      const std::vector<VkFormat>& candidates,
      VkImageTiling tiling,
      VkFormatFeatureFlags features
   );

   // Buffer Helper Functions
   void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer &buffer,
      VkDeviceMemory &buffer_memory
   );
   VkCommandBuffer beginSingleTimeCommands();
   void endSingleTimeCommands(VkCommandBuffer command_buffer);
   void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
   void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layer_count);

   void createImageWithInfo(
      const VkImageCreateInfo &image_info,
      VkMemoryPropertyFlags properties,
      VkImage &image,
      VkDeviceMemory &image_memory
   );

private:
   void createInstance();
   void setupDebugMessenger();
   void pickPhysicalDevice();
   void createLogicalDevice();
   void createCommandPool();

   // helper functions
   bool isDeviceSuitable(VkPhysicalDevice device);
   [[nodiscard]] std::vector<const char*> getRequiredExtensions() const;
   bool checkValidationLayerSupport();
   QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
   static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &create_info);
   void hasGLFWRequiredInstanceExtensions();
   bool checkDeviceExtensionSupport(VkPhysicalDevice device);
   SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

   VkInstance Instance;
   VkDebugUtilsMessengerEXT DebugMessenger;
   VkPhysicalDevice PhysicalDevice;
   VkCommandPool CommandPool;

   VkDevice Device;
   VkSurfaceKHR Surface;
   VkQueue GraphicsQueue;
   VkQueue PresentQueue;
   bool EnableValidationLayers;

   const std::vector<const char *> ValidationLayers = { "VK_LAYER_KHRONOS_validation"};
   const std::vector<const char *> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME};

   static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT message_severity_flag_bits,
      VkDebugUtilsMessageTypeFlagsEXT message_type,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
      void* user_data
   )
   {
      std::cerr << "validation layer: " << callback_data->pMessage << "\n";
      return VK_FALSE;
   }

   static VkResult createDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT* create_info,
      const VkAllocationCallbacks* allocator,
      VkDebugUtilsMessengerEXT* debug_messenger
   )
   {
      auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
      if (func != nullptr) return func( instance, create_info, allocator, debug_messenger );
      else return VK_ERROR_EXTENSION_NOT_PRESENT;
   }

   static void destroyDebugUtilsMessengerEXT(
      VkInstance instance,
      VkDebugUtilsMessengerEXT debug_messenger,
      const VkAllocationCallbacks* allocator
   )
   {
      auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
      if (func != nullptr) func( instance, debug_messenger, allocator );
   }
};