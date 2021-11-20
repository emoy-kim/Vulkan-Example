#include "vulkan_device.h"

DeviceVK::DeviceVK(GLFWwindow* window) :
   Properties{}, Instance{}, DebugMessenger{}, PhysicalDevice( VK_NULL_HANDLE ), CommandPool{}, Device{}, Surface{},
   GraphicsQueue{}, PresentQueue{}
#ifdef NDEBUG
   , EnableValidationLayers( false )
#else
   , EnableValidationLayers( true )
#endif
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

   if (EnableValidationLayers) destroyDebugUtilsMessengerEXT( Instance, DebugMessenger, nullptr);

   vkDestroySurfaceKHR( Instance, Surface, nullptr);
   vkDestroyInstance( Instance, nullptr);
}

void DeviceVK::createInstance()
{
   if (EnableValidationLayers && !checkValidationLayerSupport()) {
      std::cout << "validation layers requested, but not available!\n";
      return;
   }

   VkApplicationInfo app_info = {};
   app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app_info.pApplicationName = "LittleVulkanEngine App";
   app_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0);
   app_info.pEngineName = "No Engine";
   app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0);
   app_info.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo create_info = {};
   create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   create_info.pApplicationInfo = &app_info;

   auto extensions = getRequiredExtensions();
   create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
   create_info.ppEnabledExtensionNames = extensions.data();

   VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
   if (EnableValidationLayers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
      create_info.ppEnabledLayerNames = ValidationLayers.data();

      populateDebugMessengerCreateInfo( debug_create_info);
      create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debug_create_info;
   }
   else {
      create_info.enabledLayerCount = 0;
      create_info.pNext = nullptr;
   }

   if (vkCreateInstance( &create_info, nullptr, &Instance ) != VK_SUCCESS) {
      std::cout << "failed to create Instance!\n";
      return;
   }

   hasGLFWRequiredInstanceExtensions();
}

void DeviceVK::pickPhysicalDevice()
{
   uint32_t device_count = 0;
   vkEnumeratePhysicalDevices( Instance, &device_count, nullptr );
   if (device_count == 0) {
      std::cout << "failed to find GPUs with Vulkan support!\n";
      return;
   }

   std::cout << "Device count: " << device_count << std::endl;
   std::vector<VkPhysicalDevice> devices( device_count);
   vkEnumeratePhysicalDevices( Instance, &device_count, devices.data() );

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

   vkGetPhysicalDeviceProperties( PhysicalDevice, &Properties );
   std::cout << "physical Device: " << Properties.deviceName << "\n";
}

void DeviceVK::createLogicalDevice()
{
   QueueFamilyIndices indices = findQueueFamilies( PhysicalDevice );

   std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
   std::set<uint32_t> unique_queue_families = { indices.GraphicsFamily, indices.PresentFamily };

   float queuePriority = 1.0f;
   for (uint32_t queueFamily : unique_queue_families) {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queue_create_infos.push_back( queueCreateInfo);
   }

   VkPhysicalDeviceFeatures device_features = {};
   device_features.samplerAnisotropy = VK_TRUE;

   VkDeviceCreateInfo create_info = {};
   create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

   create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
   create_info.pQueueCreateInfos = queue_create_infos.data();

   create_info.pEnabledFeatures = &device_features;
   create_info.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
   create_info.ppEnabledExtensionNames = DeviceExtensions.data();

   // might not really be necessary anymore because Device specific validation layers have been deprecated
   if (EnableValidationLayers) {
      create_info.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
      create_info.ppEnabledLayerNames = ValidationLayers.data();
   }
   else create_info.enabledLayerCount = 0;

   if (vkCreateDevice( PhysicalDevice, &create_info, nullptr, &Device ) != VK_SUCCESS) {
      std::cout << "failed to create logical Device!\n";
      return;
   }

   vkGetDeviceQueue( Device, indices.GraphicsFamily, 0, &GraphicsQueue );
   vkGetDeviceQueue( Device, indices.PresentFamily, 0, &PresentQueue );
}

void DeviceVK::createCommandPool()
{
   QueueFamilyIndices queue_family_indices = findPhysicalQueueFamilies();

   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.queueFamilyIndex = queue_family_indices.GraphicsFamily;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

   if (vkCreateCommandPool( Device, &poolInfo, nullptr, &CommandPool ) != VK_SUCCESS) {
      std::cout << "failed to create command pool!\n";
   }
}

bool DeviceVK::isDeviceSuitable(VkPhysicalDevice device)
{
   QueueFamilyIndices indices = findQueueFamilies( device );

   bool extensions_supported = checkDeviceExtensionSupport( device );
   bool swapChainAdequate;
   if (extensions_supported) {
      SwapChainSupportDetails swapChainSupport = querySwapChainSupport( device );
      swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
   }

   VkPhysicalDeviceFeatures supportedFeatures;
   vkGetPhysicalDeviceFeatures( device, &supportedFeatures );
   return indices.isComplete() && extensions_supported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

void DeviceVK::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
{
   create_info = {};
   create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   create_info.pfnUserCallback = debugCallback;
   create_info.pUserData = nullptr;
}

void DeviceVK::setupDebugMessenger()
{
   if (!EnableValidationLayers) return;

   VkDebugUtilsMessengerCreateInfoEXT create_info;
   populateDebugMessengerCreateInfo( create_info );
   if (createDebugUtilsMessengerEXT( Instance, &create_info, nullptr, &DebugMessenger ) != VK_SUCCESS) {
      std::cout << "failed to set up debug messenger!\n";
   }
}

bool DeviceVK::checkValidationLayerSupport()
{
   uint32_t layer_count;
   vkEnumerateInstanceLayerProperties( &layer_count, nullptr );

   std::vector<VkLayerProperties> available_layers( layer_count);
   vkEnumerateInstanceLayerProperties( &layer_count, available_layers.data() );

   for (const char* layer_name : ValidationLayers) {
      bool layer_found = false;
      for (const auto& layer_properties : available_layers) {
         if (std::strcmp( layer_name, layer_properties.layerName ) == 0) {
            layer_found = true;
            break;
         }
      }
      if (!layer_found) return false;
   }
   return true;
}

std::vector<const char*> DeviceVK::getRequiredExtensions() const
{
   const char** glfw_extensions;
   uint32_t glfw_extension_count = 0;
   glfw_extensions = glfwGetRequiredInstanceExtensions( &glfw_extension_count );

   std::vector<const char*> extensions( glfw_extensions, glfw_extensions + glfw_extension_count);
   if (EnableValidationLayers) extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
   return extensions;
}

void DeviceVK::hasGLFWRequiredInstanceExtensions()
{
   uint32_t extension_count = 0;
   vkEnumerateInstanceExtensionProperties( nullptr, &extension_count, nullptr );
   std::vector<VkExtensionProperties> extensions( extension_count);
   vkEnumerateInstanceExtensionProperties( nullptr, &extension_count, extensions.data() );

   std::cout << "available extensions:\n";
   std::unordered_set<std::string> available;
   for (const auto& extension : extensions) {
      std::cout << "\t" << extension.extensionName << "\n";
      available.insert( extension.extensionName );
   }

   std::cout << "required extensions:\n";
   auto required_extensions = getRequiredExtensions();
   for (const auto &required : required_extensions) {
      std::cout << "\t" << required << "\n";
      if (available.find( required ) == available.end()) std::cout << "Missing required glfw extension\n";
   }
}

bool DeviceVK::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
   uint32_t extension_count;
   vkEnumerateDeviceExtensionProperties( device, nullptr, &extension_count, nullptr );

   std::vector<VkExtensionProperties> available_extensions(extension_count);
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extension_count,
      available_extensions.data()
   );

   std::set<std::string> required_extensions(DeviceExtensions.begin(), DeviceExtensions.end());
   for (const auto& extension : available_extensions) {
      required_extensions.erase(  extension.extensionName );
   }
   return required_extensions.empty();
}

QueueFamilyIndices DeviceVK::findQueueFamilies(VkPhysicalDevice device)
{
   QueueFamilyIndices indices;
   uint32_t queue_family_count = 0;
   vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count, nullptr );

   std::vector<VkQueueFamilyProperties> queue_families( queue_family_count);
   vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count, queue_families.data() );

   int i = 0;
   for (const auto& queue_family : queue_families) {
      if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
         indices.GraphicsFamily = i;
         indices.GraphicsFamilyHasValue = true;
      }
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR( device, i, Surface, &present_support );
      if (queue_family.queueCount > 0 && present_support) {
         indices.PresentFamily = i;
         indices.PresentFamilyHasValue = true;
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

   uint32_t format_count;
   vkGetPhysicalDeviceSurfaceFormatsKHR( device, Surface, &format_count, nullptr );

   if (format_count != 0) {
      details.formats.resize( format_count );
      vkGetPhysicalDeviceSurfaceFormatsKHR( device, Surface, &format_count, details.formats.data() );
   }

   uint32_t present_mode_count;
   vkGetPhysicalDeviceSurfacePresentModesKHR( device, Surface, &present_mode_count, nullptr );

   if (present_mode_count != 0) {
      details.presentModes.resize( present_mode_count );
      vkGetPhysicalDeviceSurfacePresentModesKHR(
         device,
         Surface,
         &present_mode_count,
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
   VkPhysicalDeviceMemoryProperties memory_properties;
   vkGetPhysicalDeviceMemoryProperties( PhysicalDevice, &memory_properties);
   for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
      if ((typeFilter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) return i;
   }
   std::cout << "failed to find suitable memory type!\n";
   return -1;
}

void DeviceVK::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& buffer_memory
 )
 {
   VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   if (vkCreateBuffer( Device, &buffer_info, nullptr, &buffer ) != VK_SUCCESS) {
      std::cout << "failed to create vertex buffer!\n";
   }

   VkMemoryRequirements memory_requirements;
   vkGetBufferMemoryRequirements( Device, buffer, &memory_requirements );

   VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   if (vkAllocateMemory( Device, &allocate_info, nullptr, &buffer_memory ) != VK_SUCCESS) {
      std::cout << "failed to allocate vertex buffer memory!\n";
   }

   vkBindBufferMemory( Device, buffer, buffer_memory, 0 );
}

VkCommandBuffer DeviceVK::beginSingleTimeCommands()
{
   VkCommandBufferAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocate_info.commandPool = CommandPool;
   allocate_info.commandBufferCount = 1;

   VkCommandBuffer command_buffer;
   vkAllocateCommandBuffers( Device, &allocate_info, &command_buffer );

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   vkBeginCommandBuffer( command_buffer, &beginInfo );
   return command_buffer;
}

void DeviceVK::endSingleTimeCommands(VkCommandBuffer command_buffer)
{
   vkEndCommandBuffer( command_buffer );

   VkSubmitInfo submit_info{};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers = &command_buffer;

   vkQueueSubmit( GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE );
   vkQueueWaitIdle( GraphicsQueue );

   vkFreeCommandBuffers( Device, CommandPool, 1, &command_buffer );
}

void DeviceVK::copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
   VkCommandBuffer command_buffer = beginSingleTimeCommands();

   VkBufferCopy copy_region{};
   copy_region.srcOffset = 0;  // Optional
   copy_region.dstOffset = 0;  // Optional
   copy_region.size = size;
   vkCmdCopyBuffer( command_buffer, src_buffer, dst_buffer, 1, &copy_region );

   endSingleTimeCommands( command_buffer );
}

void DeviceVK::copyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    uint32_t layer_count
)
{
   VkCommandBuffer command_buffer = beginSingleTimeCommands();

   VkBufferImageCopy region{};
   region.bufferOffset = 0;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;

   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount = layer_count;

   region.imageOffset = { 0, 0, 0 };
   region.imageExtent = { width, height, 1 };

   vkCmdCopyBufferToImage(
      command_buffer,
      buffer,
      image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
   );
   endSingleTimeCommands( command_buffer );
}

void DeviceVK::createImageWithInfo(
   const VkImageCreateInfo& image_info,
   VkMemoryPropertyFlags properties,
   VkImage& image,
   VkDeviceMemory& image_memory
)
{
   if (vkCreateImage( Device, &image_info, nullptr, &image ) != VK_SUCCESS) {
      std::cout << "failed to create image!\n";
   }

   VkMemoryRequirements memory_requirements;
   vkGetImageMemoryRequirements( Device, image, &memory_requirements );

   VkMemoryAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate_info.allocationSize = memory_requirements.size;
   allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   if (vkAllocateMemory( Device, &allocate_info, nullptr, &image_memory) != VK_SUCCESS) {
      std::cout << "failed to allocate image memory!\n";
   }

   if (vkBindImageMemory( Device, image, image_memory, 0) != VK_SUCCESS) {
      std::cout << "failed to bind image memory!\n";
   }
}