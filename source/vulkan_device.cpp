#include "vulkan_device.h"

// local callback functions
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
   std::cerr << "validation layer: " << pCallbackData->pMessage << "\n";
   return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
 )
 {
   auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance,
      "vkCreateDebugUtilsMessengerEXT");
   if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
   else return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
)
{
   auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance,
      "vkDestroyDebugUtilsMessengerEXT");
   if (func != nullptr) func(instance, debugMessenger, pAllocator);
}


DeviceVK::DeviceVK(GLFWwindow* window)
{
   createInstance();
   setupDebugMessenger();

   if (glfwCreateWindowSurface( Instance, window, nullptr, &Surface ) != VK_SUCCESS) {
      std::cout << "Could not create surface\n";
      return;
   }

   pickPhysicalDevice();
   createLogicalDevice();
   createCommandPool();
}

DeviceVK::~DeviceVK()
{
   vkDestroyCommandPool( Device, CommandPool, nullptr );
   vkDestroyDevice( Device, nullptr );

   if (enableValidationLayers) DestroyDebugUtilsMessengerEXT( Instance, DebugMessenger, nullptr);

   vkDestroySurfaceKHR( Instance, Surface, nullptr);
   vkDestroyInstance( Instance, nullptr);
}

void DeviceVK::createInstance()
{
   if (enableValidationLayers && !checkValidationLayerSupport()) {
      std::cout << "validation layers requested, but not available!\n";
      return;
   }

   VkApplicationInfo appInfo = {};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = "LittleVulkanEngine App";
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName = "No Engine";
   appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &appInfo;

   auto extensions = getRequiredExtensions();
   createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
   createInfo.ppEnabledExtensionNames = extensions.data();

   VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
   if (enableValidationLayers) {
      createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
      createInfo.ppEnabledLayerNames = ValidationLayers.data();

      populateDebugMessengerCreateInfo(debugCreateInfo);
      createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
   }
   else {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
   }

   if (vkCreateInstance( &createInfo, nullptr, &Instance ) != VK_SUCCESS) {
      std::cout << "failed to create Instance!\n";
      return;
   }

   hasGLFWRequiredInstanceExtensions();
}

void DeviceVK::pickPhysicalDevice()
{
   uint32_t deviceCount = 0;
   vkEnumeratePhysicalDevices( Instance, &deviceCount, nullptr );
   if (deviceCount == 0) {
      std::cout << "failed to find GPUs with Vulkan support!\n";
      return;
   }

   std::cout << "Device count: " << deviceCount << std::endl;
   std::vector<VkPhysicalDevice> devices(deviceCount);
   vkEnumeratePhysicalDevices( Instance, &deviceCount, devices.data() );

   for (const auto &device : devices) {
      if (isDeviceSuitable( device )) {
         PhysicalDevice = device;
         break;
      }
   }

   if (PhysicalDevice == VK_NULL_HANDLE) {
      std::cout << "failed to find a suitable GPU!\n";
      return;
   }

   vkGetPhysicalDeviceProperties( PhysicalDevice, &properties );
   std::cout << "physical Device: " << properties.deviceName << "\n";
}

void DeviceVK::createLogicalDevice()
{
   QueueFamilyIndices indices = findQueueFamilies( PhysicalDevice );

   std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
   std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

   float queuePriority = 1.0f;
   for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
   }

   VkPhysicalDeviceFeatures deviceFeatures = {};
   deviceFeatures.samplerAnisotropy = VK_TRUE;

   VkDeviceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

   createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
   createInfo.pQueueCreateInfos = queueCreateInfos.data();

   createInfo.pEnabledFeatures = &deviceFeatures;
   createInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
   createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

   // might not really be necessary anymore because Device specific validation layers
   // have been deprecated
   if (enableValidationLayers) {
      createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
      createInfo.ppEnabledLayerNames = ValidationLayers.data();
   }
   else createInfo.enabledLayerCount = 0;

   if (vkCreateDevice( PhysicalDevice, &createInfo, nullptr, &Device ) != VK_SUCCESS) {
      std::cout << "failed to create logical Device!\n";
      return;
   }

   vkGetDeviceQueue( Device, indices.graphicsFamily, 0, &GraphicsQueue );
   vkGetDeviceQueue( Device, indices.presentFamily, 0, &PresentQueue );
}

void DeviceVK::createCommandPool()
{
   QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

   if (vkCreateCommandPool( Device, &poolInfo, nullptr, &CommandPool ) != VK_SUCCESS) {
      std::cout << "failed to create command pool!\n";
   }
}

bool DeviceVK::isDeviceSuitable(VkPhysicalDevice device)
{
   QueueFamilyIndices indices = findQueueFamilies( device );

   bool extensionsSupported = checkDeviceExtensionSupport( device );

   bool swapChainAdequate = false;
   if (extensionsSupported) {
      SwapChainSupportDetails swapChainSupport = querySwapChainSupport( device );
      swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
   }

   VkPhysicalDeviceFeatures supportedFeatures;
   vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

   return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

void DeviceVK::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
   createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   createInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   createInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   createInfo.pfnUserCallback = debugCallback;
   createInfo.pUserData = nullptr;
}

void DeviceVK::setupDebugMessenger()
{
   if (!enableValidationLayers) return;

   VkDebugUtilsMessengerCreateInfoEXT createInfo;
   populateDebugMessengerCreateInfo( createInfo );
   if (CreateDebugUtilsMessengerEXT( Instance,  &createInfo, nullptr, &DebugMessenger ) != VK_SUCCESS) {
      std::cout << "failed to set up debug messenger!\n";
   }
}

bool DeviceVK::checkValidationLayerSupport()
{
   uint32_t layerCount;
   vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

   std::vector<VkLayerProperties> availableLayers(layerCount);
   vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

   for (const char *layerName : ValidationLayers) {
      bool layerFound = false;
      for (const auto &layerProperties : availableLayers) {
         if (std::strcmp( layerName, layerProperties.layerName ) == 0) {
            layerFound = true;
            break;
         }
      }
      if (!layerFound) return false;
   }
   return true;
}

std::vector<const char *> DeviceVK::getRequiredExtensions()
{
   const char** glfwExtensions;
   uint32_t glfwExtensionCount = 0;
   glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

   std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
   if (enableValidationLayers) extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
   return extensions;
}

void DeviceVK::hasGLFWRequiredInstanceExtensions()
{
   uint32_t extensionCount = 0;
   vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
   std::vector<VkExtensionProperties> extensions(extensionCount);
   vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );

   std::cout << "available extensions:\n";
   std::unordered_set<std::string> available;
   for (const auto& extension : extensions) {
      std::cout << "\t" << extension.extensionName << "\n";
      available.insert( extension.extensionName );
   }

   std::cout << "required extensions:\n";
   auto requiredExtensions = getRequiredExtensions();
   for (const auto &required : requiredExtensions) {
      std::cout << "\t" << required << "\n";
      if (available.find( required ) == available.end()) std::cout << "Missing required glfw extension\n";
   }
}

bool DeviceVK::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
   uint32_t extensionCount;
   vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );

   std::vector<VkExtensionProperties> availableExtensions(extensionCount);
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extensionCount,
      availableExtensions.data()
   );

   std::set<std::string> requiredExtensions( DeviceExtensions.begin(), DeviceExtensions.end());
   for (const auto& extension : availableExtensions) {
      requiredExtensions.erase(extension.extensionName);
   }
   return requiredExtensions.empty();
}

QueueFamilyIndices DeviceVK::findQueueFamilies(VkPhysicalDevice device)
{
   QueueFamilyIndices indices;
   uint32_t queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

   std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
   vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

   int i = 0;
   for (const auto &queueFamily : queueFamilies) {
      if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
         indices.graphicsFamily = i;
         indices.graphicsFamilyHasValue = true;
      }
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR( device, i, Surface, &presentSupport );
      if (queueFamily.queueCount > 0 && presentSupport) {
         indices.presentFamily = i;
         indices.presentFamilyHasValue = true;
      }
      if (indices.isComplete()) break;
      i++;
  }
  return indices;
}

SwapChainSupportDetails DeviceVK::querySwapChainSupport(VkPhysicalDevice device)
{
   SwapChainSupportDetails details;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, Surface, &details.capabilities );

   uint32_t formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR( device, Surface, &formatCount, nullptr );

   if (formatCount != 0) {
      details.formats.resize( formatCount );
      vkGetPhysicalDeviceSurfaceFormatsKHR( device, Surface, &formatCount, details.formats.data() );
   }

   uint32_t presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR( device, Surface, &presentModeCount, nullptr );

   if (presentModeCount != 0) {
      details.presentModes.resize( presentModeCount );
      vkGetPhysicalDeviceSurfacePresentModesKHR(
         device,
         Surface,
         &presentModeCount,
         details.presentModes.data()
      );
  }
  return details;
}

VkFormat DeviceVK::findSupportedFormat(
   const std::vector<VkFormat>& candidates,
   VkImageTiling tiling,
   VkFormatFeatureFlags features
)
{
   for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties( PhysicalDevice, format, &props );

      if ((tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) ||
          (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)) return format;
   }
   std::cout << "failed to find supported format!\n";
   return VK_FORMAT_UNDEFINED;
}

uint32_t DeviceVK::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties( PhysicalDevice, &memProperties);
   for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
   }
   std::cout << "failed to find suitable memory type!\n";
   return -1;
}

void DeviceVK::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
 )
 {
   VkBufferCreateInfo bufferInfo{};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = size;
   bufferInfo.usage = usage;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (vkCreateBuffer( Device, &bufferInfo, nullptr, &buffer ) != VK_SUCCESS) {
      std::cout << "failed to create vertex buffer!\n";
   }

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements( Device, buffer, &memRequirements );

   VkMemoryAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, properties );

   if (vkAllocateMemory( Device, &allocInfo, nullptr, &bufferMemory ) != VK_SUCCESS) {
      std::cout << "failed to allocate vertex buffer memory!\n";
   }

   vkBindBufferMemory( Device, buffer, bufferMemory, 0 );
}

VkCommandBuffer DeviceVK::beginSingleTimeCommands()
{
   VkCommandBufferAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = CommandPool;
   allocInfo.commandBufferCount = 1;

   VkCommandBuffer commandBuffer;
   vkAllocateCommandBuffers( Device, &allocInfo, &commandBuffer );

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer( commandBuffer, &beginInfo );
   return commandBuffer;
}

void DeviceVK::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
   vkEndCommandBuffer( commandBuffer );

   VkSubmitInfo submitInfo{};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &commandBuffer;

   vkQueueSubmit( GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
   vkQueueWaitIdle( GraphicsQueue );

   vkFreeCommandBuffers( Device, CommandPool, 1, &commandBuffer );
}

void DeviceVK::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
   VkCommandBuffer commandBuffer = beginSingleTimeCommands();

   VkBufferCopy copyRegion{};
   copyRegion.srcOffset = 0;  // Optional
   copyRegion.dstOffset = 0;  // Optional
   copyRegion.size = size;
   vkCmdCopyBuffer( commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion );

   endSingleTimeCommands( commandBuffer );
}

void DeviceVK::copyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    uint32_t layerCount
)
{
   VkCommandBuffer commandBuffer = beginSingleTimeCommands();

   VkBufferImageCopy region{};
   region.bufferOffset = 0;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;

   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount = layerCount;

   region.imageOffset = { 0, 0, 0 };
   region.imageExtent = { width, height, 1 };

   vkCmdCopyBufferToImage(
      commandBuffer,
      buffer,
      image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
   );
   endSingleTimeCommands( commandBuffer );
}

void DeviceVK::createImageWithInfo(
   const VkImageCreateInfo& imageInfo,
   VkMemoryPropertyFlags properties,
   VkImage& image,
   VkDeviceMemory& imageMemory
)
{
   if (vkCreateImage( Device, &imageInfo, nullptr, &image ) != VK_SUCCESS) {
      std::cout << "failed to create image!\n";
   }

   VkMemoryRequirements memRequirements;
   vkGetImageMemoryRequirements( Device, image, &memRequirements );

   VkMemoryAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, properties );

   if (vkAllocateMemory( Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
      std::cout << "failed to allocate image memory!\n";
   }

   if (vkBindImageMemory( Device, image, imageMemory, 0) != VK_SUCCESS) {
      std::cout << "failed to bind image memory!\n";
   }
}