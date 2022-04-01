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
   void createDescriptorPool();
   void createUniformBuffers();
   void createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout);
   void updateUniformBuffer(uint32_t current_image, VkExtent2D extent, const glm::mat4& to_world);
   [[nodiscard]] const void* getVertexData() const { return Vertices.data(); }
   [[nodiscard]] uint32_t getVertexSize() const { return static_cast<uint32_t>(Vertices.size()); }
   [[nodiscard]] VkDeviceSize getVertexBufferSize() const { return sizeof( Vertices[0] ) * Vertices.size(); };
   [[nodiscard]] VkImageView getTextureImageView() const { return TextureImageView; }
   [[nodiscard]] VkSampler getTextureSampler() const { return TextureSampler; }
   [[nodiscard]] VkDescriptorPool getDescriptorPool() const { return DescriptorPool; }
   [[nodiscard]] const VkDescriptorSet* getDescriptorSet(uint32_t index) const { return &DescriptorSets[index]; }

private:
   struct Vertex
   {
      glm::vec3 Position;
      glm::vec3 Normal;
      glm::vec2 Texture;

      Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture) :
         Position( position ), Normal( normal ), Texture( texture ) {}
   };

   struct UniformBuffer
   {
      std::vector<VkBuffer> UniformBuffers;
      std::vector<VkDeviceMemory> UniformBuffersMemory;
   };

   struct MVPUniformBufferObject
   {
       alignas(16) glm::mat4 Model;
       alignas(16) glm::mat4 View;
       alignas(16) glm::mat4 Projection;
   };

   struct MaterialUniformBufferObject
   {
      alignas(4) glm::vec4 EmissionColor;
      alignas(4) glm::vec4 AmbientColor;
      alignas(4) glm::vec4 DiffuseColor;
      alignas(4) glm::vec4 SpecularColor;
      alignas(4) float SpecularExponent;
   };

   struct LightUniformBufferObject
   {
      alignas(4) glm::vec4 Position;
      alignas(4) glm::vec4 AmbientColor;
      alignas(4) glm::vec4 DiffuseColor;
      alignas(4) glm::vec4 SpecularColor;
      alignas(4) glm::vec3 AttenuationFactors;
      alignas(4) glm::vec3 SpotlightDirection;
      alignas(4) float SpotlightExponent;
      alignas(4) float SpotlightCutoffAngle;
   };

   CommonVK* Common;
   std::vector<Vertex> Vertices;
   VkImage TextureImage;
   VkDeviceMemory TextureImageMemory;
   VkImageView TextureImageView;
   VkSampler TextureSampler;
   VkDescriptorPool DescriptorPool;
   UniformBuffer MVP;
   UniformBuffer Material;
   UniformBuffer Light;
   std::vector<VkDescriptorSet> DescriptorSets;

   static void getSquareObject(std::vector<Vertex>& vertices);
   [[nodiscard]] static VkCommandBuffer beginSingleTimeCommands();
   static void endSingleTimeCommands(VkCommandBuffer command_buffer);
   static void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
   static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
   void createTextureImage(const std::string& texture_file_path);
   void createTextureImageView();
   void createTextureSampler();
};