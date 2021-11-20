#pragma once

#include "vulkan_device.h"

class ModelVK
{
public:
   struct Vertex
   {
      glm::vec2 Position;
      glm::vec3 Color;

      Vertex(float x, float y, float r, float g, float b) : Position{ x, y }, Color{ r, g, b } {}

      static std::vector<VkVertexInputBindingDescription> getBindingDescriptions()
      {
         std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
         binding_descriptions[0].binding = 0;
         binding_descriptions[0].stride = sizeof( Vertex );
         binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
         return binding_descriptions;
      }

      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
      {
         std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
         attribute_descriptions[0].binding = 0;
         attribute_descriptions[0].location = 0;
         attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
         attribute_descriptions[0].offset = 0;

         attribute_descriptions[1].binding = 0;
         attribute_descriptions[1].location = 1;
         attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
         attribute_descriptions[1].offset = offsetof( Vertex, Color );
         return attribute_descriptions;
      }
   };

   ModelVK(DeviceVK* device, const std::vector<Vertex>& vertices);
   ~ModelVK();

   ModelVK(const ModelVK&) = delete;
   ModelVK(const ModelVK&&) = delete;
   ModelVK& operator=(const ModelVK&) = delete;
   ModelVK& operator=(const ModelVK&&) = delete;

   void bind(VkCommandBuffer command_buffer);
   void draw(VkCommandBuffer command_buffer) const;

private:
   DeviceVK* Device;
   uint32_t VertexCount;
   VkBuffer VertexBuffer;
   VkDeviceMemory VertexBufferMemory;

   void createVertexBuffers(const std::vector<Vertex>& vertices);
};