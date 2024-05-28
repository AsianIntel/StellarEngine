#pragma once

#include <glm/ext/matrix_transform.hpp>
#include "ecs/ecs.hpp"
#include <optional>
#include <vector>

import stellar.render.vulkan.plugin;
import stellar.window;
import stellar.render.primitives;
import stellar.assets.gltf;

struct App {
    flecs::world world{};

    void initialize() {
        initialize_window(world, 1280, 960);
        initialize_vulkan(world);

        world.entity("Light").set<Light>(Light {
            .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .position = glm::vec4(0.0, 2.0, 2.0, 1.0)
        });

        std::vector<flecs::entity> materials;
        std::vector<flecs::entity> meshes;
        Gltf gltf = load_gltf("../assets/archer.gltf").value();
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
    }

    void run() {
        while(world.progress()) {}
    }

    void shutdown() {
        destroy_vulkan(world);
    }

    void spawn_node(Gltf& gltf, uint32_t index, const std::optional<GlobalTransform>& parent_transform, const std::vector<flecs::entity>& materials, const std::vector<flecs::entity>& meshes) {
        GltfNode& node = gltf.nodes[index];
        flecs::entity entity = world.entity();
        if (node.mesh.has_value()) {
            GltfMesh& mesh = gltf.meshes[node.mesh.value()];
            entity.is_a(meshes[node.mesh.value()]).is_a(materials[mesh.material]);
        }
        GlobalTransform transform;
        if (parent_transform.has_value()) {
            transform = GlobalTransform { parent_transform.value().transform * node.transform };
        } else {
            transform = GlobalTransform { node.transform };
        }
        entity.set<GlobalTransform>(transform);
        for (const auto& c: node.children) {
            spawn_node(gltf, c, transform, materials, meshes);
        }
    }
};
