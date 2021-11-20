#include "vulkan_model.h"

ModelVK::ModelVK(DeviceVK* device, const std::vector<Vertex>& vertices) :
   Device( device ), VertexCount( 0 ), VertexBuffer{}, VertexBufferMemory{}
{
   createVertexBuffers( vertices );
}

ModelVK::~ModelVK()
{
   vkDestroyBuffer( Device->device(), VertexBuffer, nullptr );
   vkFreeMemory( Device->device(), VertexBufferMemory, nullptr );
}

void ModelVK::createVertexBuffers(const std::vector<Vertex>& vertices)
{
   VertexCount = static_cast<uint32_t>(vertices.size());

   assert( VertexCount >= 3 );

   VkDeviceSize buffer_size = sizeof( vertices[0] ) * VertexCount;
   Device->createBuffer(
      buffer_size,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      VertexBuffer,
      VertexBufferMemory
   );

   void* data;
   vkMapMemory( Device->device(), VertexBufferMemory, 0, buffer_size, 0, &data );
   std::memcpy( data, vertices.data(), static_cast<size_t>(buffer_size) );
   vkUnmapMemory( Device->device(), VertexBufferMemory );
}

void ModelVK::bind(VkCommandBuffer command_buffer)
{
   VkBuffer buffers[] = { VertexBuffer };
   VkDeviceSize offsets[] = { 0 };
   vkCmdBindVertexBuffers( command_buffer, 0, 1, buffers, offsets );
}

void ModelVK::draw(VkCommandBuffer command_buffer) const
{
   vkCmdDraw( command_buffer, VertexCount, 1, 0, 0 );
}