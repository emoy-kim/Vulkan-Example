#include "common.h"

bool CommonVK::checkValidationLayerSupport()
{
   uint32_t layer_count;
   vkEnumerateInstanceLayerProperties( &layer_count, nullptr );

   std::vector<VkLayerProperties> available_layers( layer_count);
   vkEnumerateInstanceLayerProperties( &layer_count, available_layers.data() );

   for (const char* layer_name : ValidationLayers) {
      bool layer_found = false;
      for (const auto& layerProperties : available_layers) {
          if (std::strcmp( layer_name, layerProperties.layerName ) == 0) {
             layer_found = true;
              break;
          }
      }
      if (!layer_found) return false;
   }
   return true;
}

CommonVK::QueueFamilyIndices CommonVK::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
   QueueFamilyIndices indices;
   uint32_t queue_family_count = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(
      device,
      &queue_family_count,
      nullptr
   );

   std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
   vkGetPhysicalDeviceQueueFamilyProperties(
      device,
      &queue_family_count,
      queue_families.data()
   );

   int i = 0;
   for (const auto& queue_family : queue_families) {
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.GraphicsFamily = i;

      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(
         device,
         i,
         surface,
         &present_support
      );
      if (present_support) indices.PresentFamily = i;
      if (indices.isComplete()) break;
      i++;
   }
   return indices;
}

bool CommonVK::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
   uint32_t extension_count;
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extension_count,
      nullptr
   );

   std::vector<VkExtensionProperties> available_extensions(extension_count);
   vkEnumerateDeviceExtensionProperties(
      device,
      nullptr,
      &extension_count,
      available_extensions.data()
   );

   std::set<std::string> required_extensions(DeviceExtensions.begin(), DeviceExtensions.end());
   for (const auto& extension : available_extensions) {
      required_extensions.erase( extension.extensionName );
   }
   return required_extensions.empty();
}

CommonVK::SwapChainSupportDetails CommonVK::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
   SwapChainSupportDetails details;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device,
      surface,
      &details.Capabilities
   );

   uint32_t format_count;
   vkGetPhysicalDeviceSurfaceFormatsKHR(
      device,
      surface,
      &format_count,
      nullptr
   );
   if (format_count != 0) {
      details.Formats.resize( format_count );
      vkGetPhysicalDeviceSurfaceFormatsKHR(
         device,
         surface,
         &format_count,
         details.Formats.data()
      );
   }

   uint32_t present_mode_count;
   vkGetPhysicalDeviceSurfacePresentModesKHR(
      device,
      surface,
      &present_mode_count,
      nullptr
   );
   if (present_mode_count != 0) {
      details.PresentModes.resize( present_mode_count );
      vkGetPhysicalDeviceSurfacePresentModesKHR(
         device,
         surface,
         &present_mode_count,
         details.PresentModes.data()
      );
   }
   return details;
}

bool CommonVK::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
   QueueFamilyIndices indices = findQueueFamilies( device, surface );
   if (!indices.isComplete()) return false;

   bool swap_chain_adequate = false;
   bool extensions_supported = checkDeviceExtensionSupport( device );
   if (extensions_supported) {
      SwapChainSupportDetails swap_chain_support = querySwapChainSupport( device, surface );
      swap_chain_adequate = !swap_chain_support.Formats.empty() && !swap_chain_support.PresentModes.empty();
   }

   VkPhysicalDeviceFeatures supported_features;
   vkGetPhysicalDeviceFeatures( device, &supported_features );
   return extensions_supported && swap_chain_adequate && supported_features.samplerAnisotropy;
}

void CommonVK::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
   uint32_t device_count = 0;
   vkEnumeratePhysicalDevices( instance, &device_count, nullptr );
   if (device_count == 0) throw std::runtime_error( "failed to find GPUs with Vulkan support!");

   std::vector<VkPhysicalDevice> devices(device_count);
   vkEnumeratePhysicalDevices(
      instance,
      &device_count,
      devices.data()
   );

   for (const auto& device : devices) {
      if (isDeviceSuitable( device, surface )) {
         PhysicalDevice = device;
         break;
      }
   }
   if (PhysicalDevice == VK_NULL_HANDLE) throw std::runtime_error("failed to find a suitable GPU!");
}

void CommonVK::createLogicalDevice(VkSurfaceKHR surface)
{
   const float queue_priority = 1.0f;
   std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
   QueueFamilyIndices indices = findQueueFamilies( PhysicalDevice, surface );
   std::set<uint32_t> unique_queue_families = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };
   for (uint32_t queue_family : unique_queue_families) {
      VkDeviceQueueCreateInfo queue_create_info{};
      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.queueFamilyIndex = queue_family;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.emplace_back( queue_create_info );
   }

   VkPhysicalDeviceFeatures device_features{};
   // write later ...

   VkDeviceCreateInfo create_info{};
   create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
   create_info.pQueueCreateInfos = queue_create_infos.data();
   create_info.pEnabledFeatures = &device_features;
   create_info.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
   create_info.ppEnabledExtensionNames = DeviceExtensions.data();

#ifdef NDBUG
   create_info.enabledLayerCount = 0;
#else
   // Vulkan made a distinction between Instance and Device specific validation layers, but this is no longer the case.
   // That means that the enabledLayerCount and ppEnabledLayerNames fields are ignored by up-to-date implementations.
   // However, it is still a good idea to set them anyway to be compatible with older implementations.
   create_info.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
   create_info.ppEnabledLayerNames = ValidationLayers.data();
#endif

   const VkResult result = vkCreateDevice(
      PhysicalDevice,
      &create_info,
      nullptr,
      &Device
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create logical Device!");

   vkGetDeviceQueue(
      Device,
      indices.GraphicsFamily.value(),
      0,
      &GraphicsQueue
   );
   vkGetDeviceQueue(
      Device,
      indices.PresentFamily.value(),
      0,
      &PresentQueue
   );
}

void CommonVK::createCommandPool(VkSurfaceKHR surface)
{
   QueueFamilyIndices queue_family_indices = findQueueFamilies( PhysicalDevice, surface );

   VkCommandPoolCreateInfo pool_info{};
   pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool_info.queueFamilyIndex = queue_family_indices.GraphicsFamily.value();

   const VkResult result = vkCreateCommandPool(
      CommonVK::getDevice(),
      &pool_info,
      nullptr,
      &CommandPool
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create command pool!");
}

VkFormat CommonVK::findSupportedFormat(
   const std::vector<VkFormat>& candidates,
   VkImageTiling tiling,
   VkFormatFeatureFlags features
)
{
   for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties( PhysicalDevice, format, &props );

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
      if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
   }
   throw std::runtime_error("failed to find supported format!");
}

VkFormat CommonVK::findDepthFormat()
{
   return findSupportedFormat(
      { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
   );
}

uint32_t CommonVK::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
   VkPhysicalDeviceMemoryProperties memory_properties;
   vkGetPhysicalDeviceMemoryProperties( PhysicalDevice, &memory_properties );
   for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
      if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
          return i;
      }
   }
   throw std::runtime_error("failed to find suitable memory type!");
}

void CommonVK::createBuffer(
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

   VkResult result =
      vkCreateBuffer( Device, &buffer_info, nullptr, &buffer );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create buffer!");

   VkMemoryRequirements memory_requirements;
   vkGetBufferMemoryRequirements( Device, buffer, &memory_requirements );

   VkMemoryAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate_info.allocationSize = memory_requirements.size;
   allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   result = vkAllocateMemory(
      Device,
      &allocate_info,
      nullptr,
      &buffer_memory
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate buffer memory!");

   vkBindBufferMemory( Device, buffer, buffer_memory, 0 );
}

void CommonVK::createImage(
   uint32_t width,
   uint32_t height,
   VkFormat format,
   VkImageTiling tiling,
   VkImageUsageFlags usage,
   VkMemoryPropertyFlags properties,
   VkImage& image,
   VkDeviceMemory& image_memory
)
{
   VkImageCreateInfo image_info{};
   image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image_info.imageType = VK_IMAGE_TYPE_2D;
   image_info.extent.width = width;
   image_info.extent.height = height;
   image_info.extent.depth = 1;
   image_info.mipLevels = 1;
   image_info.arrayLayers = 1;
   image_info.format = format;
   image_info.tiling = tiling;
   image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   image_info.usage = usage;
   image_info.samples = VK_SAMPLE_COUNT_1_BIT;
   image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VkResult result = vkCreateImage( Device, &image_info, nullptr, &image );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create image!");

   VkMemoryRequirements memory_requirements;
   vkGetImageMemoryRequirements( Device, image, &memory_requirements);

   VkMemoryAllocateInfo allocate_info{};
   allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate_info.allocationSize = memory_requirements.size;
   allocate_info.memoryTypeIndex = findMemoryType( memory_requirements.memoryTypeBits, properties );

   result = vkAllocateMemory(
      Device,
      &allocate_info,
      nullptr,
      &image_memory
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to allocate image memory!");

   vkBindImageMemory( Device, image, image_memory, 0 );
}

VkImageView CommonVK::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
   VkImageViewCreateInfo view_info{};
   view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view_info.image = image;
   view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format = format;
   view_info.subresourceRange.aspectMask = aspect_flags;
   view_info.subresourceRange.baseMipLevel = 0;
   view_info.subresourceRange.levelCount = 1;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.layerCount = 1;

   VkImageView image_view;
   const VkResult result = vkCreateImageView(
      Device,
      &view_info,
      nullptr,
      &image_view
   );
   if (result != VK_SUCCESS) throw std::runtime_error("failed to create texture image View!");
   return image_view;
}