#pragma once

#include "vulkan_model.h"

struct Transform2DComponent
{
   glm::vec2 Translation;
   glm::vec2 Scale;
   float Rotation;

   Transform2DComponent() : Translation(), Scale( 1.0f, 1.0f ), Rotation( 0.0f ) {}

   [[nodiscard]] glm::mat2 mat2() const
   {
      const float s = glm::sin( Rotation );
      const float c = glm::cos( Rotation );
      glm::mat2 rotation{ { c, s }, { -s, c } };
      glm::mat2 scale{ { Scale.x, 0.0f }, { 0.0f, Scale.y } };
      return scale * rotation;
   }
};

class GameObject
{
public:
   using id_t = unsigned int;

   std::shared_ptr<ModelVK> Model;
   glm::vec3 Color;
   Transform2DComponent Transform;

   static GameObject createGameObject()
   {
      static id_t current_id = 0;
      return GameObject{ current_id++ };
   }

   GameObject(const GameObject&) = delete;
   GameObject(GameObject&&) = default;
   GameObject& operator=(const GameObject&) = delete;
   GameObject& operator=(GameObject&&) = default;

   [[nodiscard]] id_t getID() const { return ID; }

private:
   id_t ID;

   explicit GameObject(id_t object_id);
};