module;

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>
#include "ecs/ecs.hpp"

export module stellar.scene.transform;

export struct Transform {
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
};

export struct GlobalTransform {
    glm::mat4 transform;
};

export void initialize_transform_plugin(const flecs::world& world) {
    world.system<Transform, GlobalTransform*>("Propagate Transforms")
        .term_at(1).parent().cascade().optional()
        .write<GlobalTransform>()
        .kind(flecs::PostUpdate)
        .each([](flecs::entity entity, const Transform transform, GlobalTransform* parent_transform) {
            glm::mat4 translation_mat = glm::translate(glm::mat4(1.0f), transform.translation);
            glm::mat4 rotation_mat = glm::toMat4(transform.rotation);
            glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), transform.scale);

            glm::mat4 trans = translation_mat * rotation_mat * scale_mat;

            if (parent_transform != nullptr) {
                entity.set<GlobalTransform>(GlobalTransform{
                    .transform = parent_transform->transform * trans
                });
            } else {
                entity.set<GlobalTransform>(GlobalTransform{
                    .transform = trans
                });
            }
        });
}