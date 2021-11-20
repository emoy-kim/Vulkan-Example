#pragma once

#include "game_object.h"
#include "vulkan_pipeline.h"
#include "vulkan_swap_chain.h"

struct SimplePushConstantData
{
   glm::mat2 Transform;
   glm::vec2 Offset;
   alignas(16) glm::vec3 Color;

   SimplePushConstantData() : Transform( 1.0f ), Offset(), Color() {}
};

class RendererVK
{
public:
   RendererVK();
   ~RendererVK();

   RendererVK(const RendererVK&) = delete;
   RendererVK(const RendererVK&&) = delete;
   RendererVK& operator=(const RendererVK&) = delete;
   RendererVK& operator=(const RendererVK&&) = delete;

   void play();

private:
   bool Resized;
   int FrameWidth;
   int FrameHeight;
   GLFWwindow* Window;
   std::vector<GameObject> GameObjects;
   std::shared_ptr<DeviceVK> Device;
   std::shared_ptr<PipelineVK> Pipeline;
   std::shared_ptr<SwapChainVK> SwapChain;
   VkPipelineLayout PipelineLayout;
   std::vector<VkCommandBuffer> CommandBuffers;

   static void resizeCallback(GLFWwindow* window, int width, int height);
   void loadGameObjects();
   void createPipelineLayout();
   void createPipeline();
   void createCommandBuffers();
   void freeCommandBuffers();
   void initializeVulkan();
   void recreateSwapChain();
   void recordCommandBuffer(uint32_t image_index);
   void renderGameObjects(VkCommandBuffer command_buffer);
   //void copyToHost();
   //void printVulkanInformation() const;
   void render();
};