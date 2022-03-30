#pragma once

#include "common.h"

class ObjectVK final
{
public:
   explicit ObjectVK(CommonVK* common);
   ~ObjectVK();

   void setSquareObject(const std::string& texture_file_path);

   static VkVertexInputBindingDescription getBindingDescription();
   static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
   [[nodiscard]] const void* getVertexData() const { return Vertices.data(); }
   [[nodiscard]] uint32_t getVertexSize() const { return static_cast<uint32_t>(Vertices.size()); }
   [[nodiscard]] VkDeviceSize getVertexBufferSize() const { return sizeof( Vertices[0] ) * Vertices.size(); };
   [[nodiscard]] VkImageView getTextureImageView() const { return TextureImageView; }
   [[nodiscard]] VkSampler getTextureSampler() const { return TextureSampler; }

private:
   struct Vertex
   {
      glm::vec3 Position;
      glm::vec3 Normal;
      glm::vec2 Texture;

      Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture) :
         Position( position ), Normal( normal ), Texture( texture ) {}
   };

   std::vector<Vertex> Vertices;
   CommonVK* Common;
   VkImage TextureImage;
   VkDeviceMemory TextureImageMemory;
   VkImageView TextureImageView;
   VkSampler TextureSampler;

   static void getSquareObject(std::vector<Vertex>& vertices);
   [[nodiscard]] VkCommandBuffer beginSingleTimeCommands();
   void endSingleTimeCommands(VkCommandBuffer command_buffer);
   void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
   void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
   void createTextureImage(const std::string& texture_file_path);
   void createTextureImageView();
   void createTextureSampler();
};