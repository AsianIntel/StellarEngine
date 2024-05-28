module;

#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <expected>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>

export module stellar.assets.gltf;

import stellar.render.vulkan.plugin;
import stellar.render.primitives;

export struct GltfMesh {
    Mesh mesh;
    uint32_t material;
};

export struct GltfNode {
    std::optional<uint32_t> mesh;
    glm::mat4 transform;
    std::vector<uint32_t> children;
    std::optional<uint32_t> parent;
};

export struct Gltf {
    std::vector<GltfMesh> meshes;
    std::vector<Material> materials;
    std::vector<GltfNode> nodes;
    std::vector<uint32_t> top_nodes;
};

export std::expected<Gltf, std::string> load_gltf(std::filesystem::path file_path) {
    fastgltf::Parser parser{};
    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadExternalBuffers;

    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
    fastgltf::Asset gltf;
    fastgltf::GltfType type = fastgltf::determineGltfFileType(data.get());
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::string msg { fastgltf::getErrorMessage(load.error()) };
            return std::unexpected(msg);
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadGltfBinary(data.get(), file_path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::string msg { fastgltf::getErrorMessage(load.error()) };
            return std::unexpected(msg);
        }
    } else {
        return std::unexpected("Failed to determine gltf type");
    }

    std::vector<Material> materials;
    std::vector<GltfMesh> meshes;
    std::vector<GltfNode> nodes;
    std::vector<uint32_t> top_nodes;
    
    for (fastgltf::Material& mat: gltf.materials) {
        Material& material = materials.emplace_back();
        material.color[0] = mat.pbrData.baseColorFactor[0];
        material.color[1] = mat.pbrData.baseColorFactor[1];
        material.color[2] = mat.pbrData.baseColorFactor[2];
        material.color[3] = mat.pbrData.baseColorFactor[3];
    }

    for (fastgltf::Mesh& gltf_mesh: gltf.meshes) {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        for (auto&& p: gltf_mesh.primitives) {
            size_t initial_vertex = vertices.size();
            {
                fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
                fastgltf::iterateAccessor<uint32_t>(gltf, index_accessor, [&](uint32_t idx) {
                    indices.push_back(idx + initial_vertex); 
                });
            }
            {
                fastgltf::Accessor& position_accessor = gltf.accessors[p.findAttribute("POSITION")->second];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, position_accessor, [&](const glm::vec3 v, size_t index) {
                    const Vertex vertex {
                        .position = glm::vec4(v, 1.0f),
                        .normal = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
                        .uv = glm::vec2(0.0f, 0.0f)
                    };
                    vertices.push_back(vertex);
                });
            }

            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second], [&](glm::vec3 v, size_t index) {
                    vertices[initial_vertex + index].normal = glm::vec4(v, 0.0f); 
                });
            }

            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second], [&](glm::vec2 v, size_t index) {
                    vertices[initial_vertex + index].uv = v; 
                });
            }
        }

        meshes.push_back(GltfMesh {
            .mesh = Mesh {
                .vertices = vertices,
                .indices = indices
            },
            .material = static_cast<uint32_t>(gltf_mesh.primitives[0].materialIndex.value_or(0))
        });
    }

    for (fastgltf::Node& gltf_node: gltf.nodes) {
        GltfNode& node = nodes.emplace_back();
        if (gltf_node.meshIndex.has_value()) {
            node.mesh = gltf_node.meshIndex.value();
        }

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) { memcpy(&node.transform, matrix.data(), sizeof(matrix)); },
                [&](fastgltf::TRS transform) {
                    glm::vec3 translation(transform.translation[0], transform.translation[1], transform.translation[1]);
                    glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);

                    glm::mat4 translation_mat = glm::translate(glm::mat4(1.0f), translation);
                    glm::mat4 rotation_mat = glm::toMat4(rotation);
                    glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);

                    node.transform = translation_mat * rotation_mat * scale_mat;
                }
            },
            gltf_node.transform
        );
    }

    for (uint32_t i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        for (auto& c: node.children) {
            nodes[i].children.push_back(c);
            nodes[i].parent = i;
        }
    }

    for (uint32_t i = 0; i < nodes.size(); i++) {
        if (!nodes[i].parent.has_value()) {
            top_nodes.push_back(i);
        }
    }

    return Gltf {
        .meshes = meshes,
        .materials = materials,
        .nodes = nodes,
        .top_nodes = top_nodes
    };
}