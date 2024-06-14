module;

#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#pragma warning(disable : 4267 4244)

export module stellar.assets.gltf;

import stellar.render.vulkan.plugin;
import stellar.render.primitives;
import stellar.animation;
import stellar.scene.transform;
import stellar.core.result;
import stellar.core;
import stellar.render.types;

export struct GltfMesh {
    Mesh mesh;
    uint32_t material;
};

export struct GltfNode {
    std::optional<uint32_t> mesh;
    std::optional<uint32_t> joint;
    std::optional<uint32_t> skin;
    Transform transform;
    std::vector<uint32_t> children;
    std::optional<uint32_t> parent;
};

export struct GltfJoint {
    Joint joint;
    uint32_t node_index;
};

export struct GltfSampler {
    Filter min_filter;
    Filter mag_filter;
};

export struct GltfMaterial {
    glm::vec4 color;
    std::optional<uint32_t> color_texture_index;
    std::optional<uint32_t> color_sampler_index;
};

export struct Gltf {
    std::vector<GltfMesh> meshes;
    std::vector<GltfMaterial> materials;
    std::vector<GltfNode> nodes;
    std::vector<GltfJoint> joints;
    std::vector<AnimationClip> animations;
    std::vector<uint32_t> top_nodes;
    std::vector<GltfSampler> samplers;
    std::vector<CPUTexture> textures;
};

export Result<Gltf, std::string> load_gltf(std::filesystem::path file_path) {
    fastgltf::Parser parser{};
    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::LoadExternalImages;

    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
    fastgltf::Asset gltf;
    fastgltf::GltfType type = fastgltf::determineGltfFileType(data.get());
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::string msg { fastgltf::getErrorMessage(load.error()) };
            return Err(msg);
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadGltfBinary(data.get(), file_path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::string msg { fastgltf::getErrorMessage(load.error()) };
            return Err(msg);
        }
    } else {
        return Err(std::string("Failed to determine gltf type"));
    }

    std::vector<GltfMaterial> materials;
    std::vector<GltfMesh> meshes;
    std::vector<GltfNode> nodes;
    std::vector<uint32_t> top_nodes;
    std::vector<GltfJoint> joints;
    std::vector<AnimationClip> animations;
    std::vector<GltfSampler> samplers;
    std::vector<CPUTexture> textures;

    for (fastgltf::Sampler& sampler: gltf.samplers) {
        Filter min_filter;
        switch (sampler.minFilter.value()) {
            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapLinear:
            case fastgltf::Filter::LinearMipMapNearest:
                min_filter = Filter::Linear;
                break;
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::NearestMipMapNearest:
                min_filter = Filter::Nearest;
                break;
        }
        Filter mag_filter;
        switch (sampler.magFilter.value()) {
            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapLinear:
            case fastgltf::Filter::LinearMipMapNearest:
                mag_filter = Filter::Linear;
            break;
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::NearestMipMapNearest:
                mag_filter = Filter::Nearest;
            break;
        }
        samplers.push_back(GltfSampler {
            .min_filter = min_filter,
            .mag_filter = mag_filter
        });
    }

    for (fastgltf::Texture& texture: gltf.textures) {
        const fastgltf::Image& image = gltf.images[texture.imageIndex.value()];
        std::visit(fastgltf::visitor {
            [&](fastgltf::sources::BufferView view) {
                const fastgltf::BufferView& buffer_view = gltf.bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer = gltf.buffers[buffer_view.bufferIndex];

                std::visit(fastgltf::visitor {
                    [&](const fastgltf::sources::Array& array) {
                        int width, height, channels;
                        uint8_t* buffer_data = stbi_load_from_memory(array.bytes.data() + buffer_view.byteOffset, buffer_view.byteLength, &width, &height, &channels, 4);
                        if (buffer_data) {
                            std::vector<uint8_t> image_data(width * height * 4);
                            memcpy(image_data.data(), buffer_data, width * height * 4);
                            stbi_image_free(buffer_data);
                            textures.push_back(CPUTexture {
                                .data = std::move(image_data),
                                .width = static_cast<uint32_t>(width),
                                .height = static_cast<uint32_t>(height)
                            });
                        }
                    },
                    [](auto& arg) { unreachable(); }
                }, buffer.data);
            },
            [](auto& arg) { unreachable(); }
        }, image.data);
    }
    
    for (fastgltf::Animation& animation: gltf.animations) {
        std::vector<std::vector<AnimationCurve>> curves(gltf.nodes.size());
        float duration = 0;
        for (fastgltf::AnimationChannel& channel: animation.channels) {
            fastgltf::AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
            fastgltf::Accessor& output_accessor = gltf.accessors[sampler.outputAccessor];
            fastgltf::Accessor& input_accessor = gltf.accessors[sampler.inputAccessor];
            
            AnimationCurve& curve = curves[channel.nodeIndex.value()].emplace_back();

            fastgltf::iterateAccessor<float>(gltf, input_accessor, [&](float v) {
                curve.keyframe_timestamps.push_back(v);
            });
            duration = std::max(*std::ranges::max_element(curve.keyframe_timestamps.begin(), curve.keyframe_timestamps.end()), duration);
            
            if (channel.path == fastgltf::AnimationPath::Rotation) {
                std::vector<glm::quat> rotations;
                fastgltf::iterateAccessor<glm::vec4>(gltf, output_accessor, [&](glm::vec4 v) {
                    glm::quat quat(v[3], v[0], v[1], v[2]);
                    rotations.push_back(quat);
                });
                curve.keyframes.frames = Keyframes::Rotation { rotations };
            } else if (channel.path == fastgltf::AnimationPath::Scale) {
                std::vector<glm::vec3> scales;
                fastgltf::iterateAccessor<glm::vec3>(gltf, output_accessor, [&](glm::vec3 v) {
                    scales.push_back(v); 
                });
                curve.keyframes.frames = Keyframes::Scale { scales };
            } else if (channel.path == fastgltf::AnimationPath::Translation) {
                std::vector<glm::vec3> translations;
                fastgltf::iterateAccessor<glm::vec3>(gltf, output_accessor, [&](glm::vec3 v) {
                    translations.push_back(v);
                });
                curve.keyframes.frames = Keyframes::Translation { translations };
            }

            if (sampler.interpolation == fastgltf::AnimationInterpolation::Linear) {
                curve.interpolation = Interpolation::Linear;
            } else if (sampler.interpolation == fastgltf::AnimationInterpolation::Step) {
                curve.interpolation = Interpolation::Step;
            } else if (sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline) {
                curve.interpolation = Interpolation::CubicSpline;
            }
        }
        animations.push_back(AnimationClip {
            .curves = curves,
            .duration = duration
        });
    }
    
    for (fastgltf::Material& mat: gltf.materials) {
        GltfMaterial& material = materials.emplace_back();
        material.color[0] = mat.pbrData.baseColorFactor[0];
        material.color[1] = mat.pbrData.baseColorFactor[1];
        material.color[2] = mat.pbrData.baseColorFactor[2];
        material.color[3] = mat.pbrData.baseColorFactor[3];
        if (mat.pbrData.baseColorTexture.has_value()) {
            material.color_texture_index = mat.pbrData.baseColorTexture.value().textureIndex;
            material.color_sampler_index = gltf.textures[material.color_texture_index.value()].samplerIndex.value();
        }
    }

    for (fastgltf::Skin& skin: gltf.skins) {
        fastgltf::Accessor& joint_accessor = gltf.accessors[skin.inverseBindMatrices.value()];
        fastgltf::iterateAccessorWithIndex<glm::mat4>(gltf, joint_accessor, [&](const glm::mat4 m, size_t index) {
             joints.push_back(GltfJoint {
                .joint = Joint {
                    .inverse_bind = m,
                    .buffer_offset = static_cast<uint32_t>(index)
                },
                .node_index = static_cast<uint32_t>(skin.joints[index])
             });
        });
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

            auto joints_attribute = p.findAttribute("JOINTS_0");
            if (joints_attribute != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::uvec4>(gltf, gltf.accessors[joints_attribute->second], [&](glm::uvec4 v, size_t index) {
                     vertices[initial_vertex + index].joints = v;
                });
            }

            auto weights = p.findAttribute("WEIGHTS_0");
            if (weights != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[weights->second], [&](glm::vec4 v, size_t index) {
                    vertices[initial_vertex + index].weights = v;
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

    for (uint32_t i = 0; i < gltf.nodes.size(); i++) {
        GltfNode& node = nodes.emplace_back();
        if (gltf.nodes[i].meshIndex.has_value()) {
            node.mesh = gltf.nodes[i].meshIndex.value();
        }
        if (gltf.nodes[i].skinIndex.has_value()) {
            node.skin = gltf.nodes[i].skinIndex.value();
        }

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) {
                    glm::mat4 mat;
                    memcpy(&mat, matrix.data(), sizeof(matrix));
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(mat, node.transform.scale, node.transform.rotation, node.transform.scale, skew, perspective);
                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 translation(transform.translation[0], transform.translation[1], transform.translation[2]);
                    glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);

                    node.transform.translation = translation;
                    node.transform.rotation = rotation;
                    node.transform.scale = scale;
                }
            },
            gltf.nodes[i].transform
        );

        auto it = std::ranges::find_if(joints.begin(), joints.end(), [&](const GltfJoint& joint) { return joint.node_index == i; });
        if (it != joints.end()) {
            node.joint = std::distance(joints.begin(), it);
        }
    }

    for (uint32_t i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        for (auto& c: node.children) {
            nodes[i].children.push_back(c);
            nodes[c].parent = i;
        }
    }

    for (uint32_t i = 0; i < nodes.size(); i++) {
        if (!nodes[i].parent.has_value()) {
            top_nodes.push_back(i);
        }
    }

    return Ok(Gltf {
        .meshes = meshes,
        .materials = materials,
        .nodes = nodes,
        .joints = joints,
        .animations = animations,
        .top_nodes = top_nodes,
        .samplers = samplers,
        .textures = textures,
    });
}