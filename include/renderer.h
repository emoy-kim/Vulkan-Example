#pragma once

#include "object.h"
#include "shader.h"

struct UniformBufferObject
{
    alignas(16) glm::mat4 Model;
    alignas(16) glm::mat4 View;
    alignas(16) glm::mat4 Projection;
};

class RendererVK final
{
public:
   RendererVK();
   ~RendererVK();

   void play();

private:
   uint32_t FrameWidth;
   uint32_t FrameHeight;
   int MaxFramesInFlight;
   std::shared_ptr<CommonVK> Common;
   GLFWwindow* Window;
   VkInstance Instance;
   VkSurfaceKHR Surface;
   VkSwapchainKHR SwapChain;
   std::vector<VkImage> SwapChainImages;
   VkFormat SwapChainImageFormat;
   VkExtent2D SwapChainExtent;
   std::vector<VkImageView> SwapChainImageViews;
   std::vector<VkFramebuffer> SwapChainFramebuffers;
   VkImage DepthImage;
   VkDeviceMemory DepthImageMemory;
   VkImageView DepthImageView;
   VkBuffer VertexBuffer;
   VkDeviceMemory VertexBufferMemory;
   std::vector<VkBuffer> UniformBuffers;
   std::vector<VkDeviceMemory> UniformBuffersMemory;
   VkDescriptorPool DescriptorPool;
   std::vector<VkDescriptorSet> DescriptorSets;
   std::vector<VkCommandBuffer> CommandBuffers;
   std::vector<VkSemaphore> ImageAvailableSemaphores;
   std::vector<VkSemaphore> RenderFinishedSemaphores;
   std::vector<VkFence> InFlightFences;
   uint32_t CurrentFrame;
   bool FramebufferResized;
   std::shared_ptr<ObjectVK> SquareObject;
   std::shared_ptr<ShaderVK> Shader;

#ifdef NDEBUG
   inline static constexpr bool EnableValidationLayers = false;
#else
   inline static constexpr bool EnableValidationLayers = true;
   VkDebugUtilsMessengerEXT DebugMessenger{};

   static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info);
   void setupDebugMessenger();
   static VkResult createDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT* create_info,
      const VkAllocationCallbacks* allocator,
      VkDebugUtilsMessengerEXT* debug_messenger
   )
   {
       auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
         instance,
         "vkCreateDebugUtilsMessengerEXT"
      );
       if (func != nullptr) return func( instance, create_info, allocator, debug_messenger );
       else return VK_ERROR_EXTENSION_NOT_PRESENT;
   }

   static void destroyDebugUtilsMessengerEXT(
      VkInstance instance,
      VkDebugUtilsMessengerEXT debug_messenger,
      const VkAllocationCallbacks* allocator
   )
   {
      auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
         instance,
         "vkDestroyDebugUtilsMessengerEXT"
      );
      if (func != nullptr) func( instance, debug_messenger, allocator);
   }

   static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
      VkDebugUtilsMessageTypeFlagsEXT message_type,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
      void* user_data
   )
   {
      std::cerr << "validation layer: " << callback_data->pMessage << "\n";
      return VK_FALSE;
   }
#endif
   static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
   {
      reinterpret_cast<RendererVK*>(glfwGetWindowUserPointer( window ))->FramebufferResized = true;
   }

   void cleanupSwapChain();
   void initializeWindow();
   void createSurface();
   [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
   [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
   [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
   void createSwapChain();
   void createImageViews();
   void createObject();
   void createGraphicsPipeline();
   void createDepthResources();
   void createFramebuffers();
   static void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
   void createVertexBuffer();
   void createUniformBuffers();
   void createDescriptorPool();
   void createDescriptorSets();
   void createCommandBuffer();
   void createSyncObjects();
   void initializeVulkan();
   void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);
   void recreateSwapChain();
   void updateUniformBuffer(uint32_t current_image);
   void drawFrame();
   [[nodiscard]] static std::vector<const char*> getRequiredExtensions();
   void createInstance();
};