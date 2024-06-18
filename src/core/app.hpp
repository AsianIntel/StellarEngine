#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "ecs/ecs.hpp"
#include <optional>
#include <vector>
#include <unordered_map>

import stellar.render.vulkan.plugin;
import stellar.window;
import stellar.render.primitives;
import stellar.assets.gltf;
import stellar.animation;
import stellar.scene.transform;
import stellar.core.result;
import stellar.input.keyboard;

struct Character {};

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
        });

        std::vector<flecs::entity> materials;
        std::vector<flecs::entity> meshes;
        std::vector<flecs::entity> textures;
        std::vector<flecs::entity> samplers;
        Gltf gltf = load_gltf("../../assets/archer.glb").unwrap();
        for (const auto& sampler: gltf.samplers) {
            flecs::entity entity = world.entity().set<CPUSampler>(CPUSampler { .min_filter = sampler.min_filter, .mag_filter = sampler.mag_filter });
            samplers.push_back(entity);
        }
        for (const auto& texture: gltf.textures) {
            flecs::entity entity = world.entity().set<CPUTexture>(texture);
            textures.push_back(entity);
        }
        for (const auto& gltf_material: gltf.materials) {
            Material material { .color = gltf_material.color };
            if (gltf_material.color_texture_index.has_value()) {
                material.color_texture = textures[gltf_material.color_texture_index.value()];
                material.color_sampler = samplers[gltf_material.color_sampler_index.value()];
            }
            flecs::entity entity = world.entity().set<Material>(material);
            materials.push_back(entity);
        }
        for (const auto& mesh: gltf.meshes) {
            flecs::entity entity = world.entity().set<Mesh>(mesh.mesh);
            meshes.push_back(entity);
        }

        std::vector<flecs::entity> joints;
        std::unordered_map<flecs::entity, uint32_t> entity_to_skin;
        std::vector<flecs::entity> top_entities;
        for (const auto& index: gltf.top_nodes) {
            auto entity = spawn_node(gltf, index, std::nullopt, materials, meshes, joints, entity_to_skin);
            top_entities.push_back(entity);
        }
        for (std::pair<flecs::entity, uint32_t> it: entity_to_skin) {
            it.first.set<SkinnedMesh>(SkinnedMesh {
                .joints = joints
            });
        }
        top_entities[1].add<Character>();

        flecs::entity animation = world.entity().set<AnimationClip>(gltf.animations[0]);
        flecs::entity player = world.entity<AnimationPlayer>().set<AnimationPlayer>(AnimationPlayer {
            .animation = animation,
            .active_animation = ActiveAnimation {
                .speed = 1.0,
                .playing = false,
                .seek_time = 0,
            }
        });

        world.observer<Window>()
            .term_at(0).singleton()
            .event<KeyboardEvent>()
            .each([=](flecs::iter& it, size_t i, Window& window) {
                KeyboardEvent* event = static_cast<KeyboardEvent*>(it.param());
                it.world().event<KeyboardEvent>().id<AnimationPlayer>().ctx(*event).entity(player).emit();
                it.world().event<KeyboardEvent>().id<Character>().ctx(*event).entity(top_entities[1]).emit();
            });

        world.observer<AnimationPlayer>()
            .term_at(0).singleton()
            .event<KeyboardEvent>()
            .each([](flecs::iter& it, size_t i, AnimationPlayer& player) {
                KeyboardEvent* event = static_cast<KeyboardEvent*>(it.param());
                if (event->key == Key::KeyW) {
                    if (event->state == KeyState::Pressed) {
                        player.play();
                    } else {
                        player.pause();
                    }
                }
            });

        world.observer<Transform>().with<Character>()
            .event<KeyboardEvent>()
            .each([](flecs::iter& it, size_t i, Transform& transform) {
                KeyboardEvent* event = static_cast<KeyboardEvent*>(it.param());
                if (event->key == Key::KeyW) {
                    transform.translation.z += (15.0f * it.world().delta_time());
                }
            });

        world.entity("Camera")
            .set<Camera>(Camera {
                .projection = glm::perspectiveLH(glm::radians(60.0f), 1280.0f / 960.0f, 10000.0f, 0.01f)
            })
            .set<Transform>(Transform {
                .translation = glm::vec3(0.0, -500.0, -500.0),
                .rotation = glm::quatLookAtLH(glm::normalize(glm::vec3(0.0, 500.0, 500.0)), glm::vec3(0.0, 0.0, -1.0)),
                .scale = glm::vec3(1.0, 1.0, 1.0)
            })
            .child_of(top_entities[1]);
    }

    void run() {
        while(world.progress()) {}
    }

    void shutdown() {
        destroy_vulkan(world);
    }

    flecs::entity spawn_node(
        Gltf& gltf,
        uint32_t index,
        const std::optional<flecs::entity>& parent,
        const std::vector<flecs::entity>& materials,
        const std::vector<flecs::entity>& meshes,
        std::vector<flecs::entity>& joints,
        std::unordered_map<flecs::entity, uint32_t>& entity_to_skin
    ) {
        GltfNode& node = gltf.nodes[index];
        flecs::entity entity = world.entity();
        if (node.mesh.has_value()) {
            GltfMesh& mesh = gltf.meshes[node.mesh.value()];
            entity.is_a(meshes[node.mesh.value()]).is_a(materials[mesh.material]);
        }
        if (node.joint.has_value()) {
            GltfJoint& joint = gltf.joints[node.joint.value()];
            entity.set(joint.joint);
            joints.push_back(entity);
        }
        if (node.skin.has_value()) {
            entity_to_skin.insert({ entity, node.skin.value() });
        }
        
        if (parent.has_value()) {
            entity.child_of(parent.value());
        }
        
        entity.set<Transform>(node.transform).set<AnimationTarget>(AnimationTarget { index });
        for (const auto& c: node.children) {
            spawn_node(gltf, c, entity, materials, meshes, joints, entity_to_skin);
        }
        return entity;
    }
};
