#pragma once

#include <base.h>

struct Vertex
{
   glm::vec3 Position;
   glm::vec3 Color;
   glm::vec2 TexCoord;

   static VkVertexInputBindingDescription getBindingDescription()
   {
      VkVertexInputBindingDescription binding_description{};
      binding_description.binding = 0;
      binding_description.stride = sizeof( Vertex );
      binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
      return binding_description;
   }

   static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
   {
      std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

      attribute_descriptions[0].binding = 0;
      attribute_descriptions[0].location = 0;
      attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      attribute_descriptions[0].offset = offsetof( Vertex, Position );

      attribute_descriptions[1].binding = 0;
      attribute_descriptions[1].location = 1;
      attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      attribute_descriptions[1].offset = offsetof( Vertex, Color );

      attribute_descriptions[2].binding = 0;
      attribute_descriptions[2].location = 2;
      attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
      attribute_descriptions[2].offset = offsetof( Vertex, TexCoord );
      return attribute_descriptions;
    }
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 Model;
    alignas(16) glm::mat4 View;
    alignas(16) glm::mat4 Projection;
};

const std::vector<Vertex> Vertices =
{
    { {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
    { {0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
    { {0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
    { {-0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },

    { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
    { { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
    { { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
};

const std::vector<uint16_t> Indices =
{
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

class RendererVK final
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

   inline static constexpr uint32_t WIDTH = 800;
   inline static constexpr uint32_t HEIGHT = 600;
   inline static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

   RendererVK();
   ~RendererVK() = default;

   void play();

private:
   GLFWwindow* Window;
   VkInstance Instance;
   VkSurfaceKHR Surface;
   VkPhysicalDevice PhysicalDevice;
   VkDevice Device;
   VkQueue GraphicsQueue;
   VkQueue PresentQueue;
   VkSwapchainKHR SwapChain;
   std::vector<VkImage> SwapChainImages;
   VkFormat SwapChainImageFormat;
   VkExtent2D SwapChainExtent;
   std::vector<VkImageView> SwapChainImageViews;
   std::vector<VkFramebuffer> SwapChainFramebuffers;
   VkRenderPass RenderPass;
   VkDescriptorSetLayout DescriptorSetLayout;
   VkPipelineLayout PipelineLayout;
   VkPipeline GraphicsPipeline;
   VkCommandPool CommandPool;
   VkImage DepthImage;
   VkDeviceMemory DepthImageMemory;
   VkImageView DepthImageView;
   VkImage TextureImage;
   VkDeviceMemory TextureImageMemory;
   VkImageView TextureImageView;
   VkSampler TextureSampler;
   VkBuffer VertexBuffer;
   VkDeviceMemory VertexBufferMemory;
   VkBuffer IndexBuffer;
   VkDeviceMemory IndexBufferMemory;
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

   inline static const std::array<const char*, 1> ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"
   };
   inline static const std::array<const char*, 1> DeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
   };
#ifdef NDEBUG
   inline static constexpr bool EnableValidationLayers = false;
#else
   inline static constexpr bool EnableValidationLayers = true;
   VkDebugUtilsMessengerEXT DebugMessenger{};

   static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info);
   void setupDebugMessenger();
   [[nodiscard]] static bool checkValidationLayerSupport();
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

   void initializeWindow();
   void createSurface();
   [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
   [[nodiscard]] static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
   [[nodiscard]] SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
   [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device);
   void pickPhysicalDevice();
   void createLogicalDevice();
   [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
   [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
   [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
   void createSwapChain();
   void createImageViews();
   void createRenderPass();
   void createDescriptorSetLayout();
   [[nodiscard]] static std::vector<char> readFile(const std::string& filename);
   [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code);
   void createGraphicsPipeline();
   void createCommandPool();
   [[nodiscard]] VkFormat findSupportedFormat(
      const std::vector<VkFormat>& candidates,
      VkImageTiling tiling,
      VkFormatFeatureFlags features
   );
   [[nodiscard]] VkFormat findDepthFormat();
   [[nodiscard]] static bool hasStencilComponent(VkFormat format);
   void createImage(
      uint32_t width,
      uint32_t height,
      VkFormat format,
      VkImageTiling tiling,
      VkImageUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkImage& image,
      VkDeviceMemory& image_memory
   );
   VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
   void createDepthResources();
   void createFramebuffers();
   VkCommandBuffer beginSingleTimeCommands();
   void endSingleTimeCommands(VkCommandBuffer command_buffer);
   void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
   void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
   void createTextureImage();
   void createTextureImageView();
   void createTextureSampler();
   [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
   void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer& buffer,
      VkDeviceMemory& buffer_memory
   );
   void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
   void createVertexBuffer();
   void createIndexBuffer();
   void createUniformBuffers();
   void createDescriptorPool();
   void createDescriptorSets();
   void createCommandBuffer();
   void createSyncObjects();
   void initializeVulkan();
   void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);
   void cleanupSwapChain();
   void recreateSwapChain();
   void updateUniformBuffer(uint32_t current_image);
   void drawFrame();
   void mainLoop();
   void cleanup();
   [[nodiscard]] static std::vector<const char*> getRequiredExtensions();
   void createInstance();
};