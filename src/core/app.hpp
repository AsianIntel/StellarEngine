#pragma once

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "ecs/ecs.hpp"
#include <optional>
#include <vector>

import stellar.render.vulkan.plugin;
import stellar.window;
import stellar.render.primitives;
import stellar.assets.gltf;
import stellar.animation;
import stellar.scene.transform;
import stellar.core.result;

struct App {
    flecs::world world{};

    void initialize() {
        flecs::log::set_level(2);

        initialize_window(world, 1280, 960);
        initialize_vulkan(world);
        initialize_animation_plugin(world);
        initialize_transform_plugin(world);

        world.entity("Light").set<Light>(Light {
            .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .position = glm::vec4(0.0, 2.0, 2.0, 1.0)
        });

        std::vector<flecs::entity> materials;
        std::vector<flecs::entity> meshes;
        Gltf gltf = load_gltf("../../assets/archer.glb").unwrap();
        for (const auto& material: gltf.materials) {
            flecs::entity entity = world.entity().set<Material>(material);
            materials.push_back(entity);
        }
        for (const auto& mesh: gltf.meshes) {
            flecs::entity entity = world.entity().set<Mesh>(mesh.mesh);
            meshes.push_back(entity);
        }
        for (const auto& index: gltf.top_nodes) {
            spawn_node(gltf, index, std::nullopt, materials, meshes);
        }

        flecs::entity animation = world.entity().set<AnimationClip>(gltf.animations[0]);
        world.set<AnimationPlayer>(AnimationPlayer {
            .animation = animation,
            .active_animation = ActiveAnimation {
                .speed = 1.0,
                .playing = true,
                .seek_time = 0,
            }
        });
    }

    void run() {
        while(world.progress()) {}
    }

    void shutdown() {
        destroy_vulkan(world);
    }

    void spawn_node(Gltf& gltf, uint32_t index, const std::optional<flecs::entity>& parent, const std::vector<flecs::entity>& materials, const std::vector<flecs::entity>& meshes) {
        GltfNode& node = gltf.nodes[index];
        flecs::entity entity = world.entity();
        if (node.mesh.has_value()) {
            GltfMesh& mesh = gltf.meshes[node.mesh.value()];
            entity.is_a(meshes[node.mesh.value()]).is_a(materials[mesh.material]);
        }
        if (node.joint.has_value()) {
            GltfJoint& joint = gltf.joints[node.joint.value()];
            entity.set(joint.joint);
        }
        
        if (parent.has_value()) {
            entity.child_of(parent.value());
        }
        
        entity.set<Transform>(node.transform).set<AnimationTarget>(AnimationTarget { index });
        for (const auto& c: node.children) {
            spawn_node(gltf, c, entity, materials, meshes);
        }
    }
};
