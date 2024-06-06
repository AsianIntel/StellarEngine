module;

#include "ecs/ecs.hpp"
#include <array>
#include <vulkan/vulkan.hpp>
#include <optional>
#include <fstream>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/packing.hpp>
#include <iostream>

#pragma warning(disable: 4267)

export module stellar.render.vulkan.plugin;

import stellar.render.vulkan;
import stellar.render.types;
import stellar.render.primitives;
import stellar.window;
import stellar.scene.transform;
import stellar.core.result;

std::string read_file(const std::string& filename);

struct ViewUniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec4 position;
};

struct GPUMesh {
    uint32_t vertex_count;
    uint32_t vertex_offset;
    std::optional<uint32_t> index_count;
    std::optional<uint32_t> index_offset;
};

export struct Material {
    glm::vec4 color;
    flecs::entity color_texture;
    flecs::entity color_sampler;
};

struct GPUMaterial {
    glm::vec4 color;
    uint32_t color_texture;
    uint32_t color_sampler;
    glm::vec2 padding;
};

export struct Light {
    glm::vec4 color;
    glm::vec4 position;
};

export struct Joint {
    glm::mat4 inverse_bind;
    uint32_t buffer_offset;
};

export struct CPUTexture {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
};

export struct GPUTexture {
    Texture texture;
    TextureView view;
    uint32_t binding;
};

export struct CPUSampler {
    Filter min_filter;
    Filter mag_filter;
};

export struct GPUSampler {
    Sampler sampler;
    uint32_t binding;
};

template<typename T>
struct DynamicUniformIndex {
    uint32_t offset;
};

struct RenderContext {
    Extent3d extent{};
    Instance instance{};
    Adapter adapter{};
    Device device{};
    Queue queue{};
    Surface surface{};
    CommandEncoder encoder{};
    Fence render_fence{};
    Semaphore swapchain_semaphore{};
    Semaphore render_semaphore{};

    Pipeline pipeline{};
    Buffer vertex_buffer{};
    Buffer index_buffer{};
    Buffer view_buffer{};
    Buffer material_buffer{};
    Buffer transform_buffer{};
    Buffer light_buffer{};
    Buffer joint_buffer{};
    Texture depth_texture{};
    TextureView depth_texture_view{};

    uint32_t vertex_buffer_index{};
    uint32_t view_buffer_index{};
    uint32_t material_buffer_index{};
    uint32_t transform_buffer_index{};
    uint32_t light_buffer_index{};
    uint32_t joint_buffer_index{};
};

void render(flecs::iter& it) {
    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    
    SurfaceTexture surface_texture = context->surface.acquire_texture(context->swapchain_semaphore).unwrap();
    context->encoder.begin_encoding().unwrap();

    {
        std::array barriers {
            TextureBarrier {
                .texture = &surface_texture.texture,
                .range = ImageSubresourceRange {
                    .aspect = FormatAspect::Color,
                    .base_mip_level = 0,
                    .mip_level_count = 1,
                    .base_array_layer = 0,
                    .array_layer_count = 1
                },
                .before = TextureUsage::Undefined,
                .after = TextureUsage::RenderTarget
            }
        };
        context->encoder.transition_textures(barriers);
    }

    std::array color_attachments {
        ColorAttachment {
            .target = Attachment { .view = &surface_texture.view },
            .ops = AttachmentOps::Store,
            .clear = Color { 0.0, 0.0, 0.0, 1.0 }
        }
    };
    DepthAttachment depth_attachment {
        .target = Attachment { .view = &context->depth_texture_view },
        .ops = AttachmentOps::Store,
        .depth_clear = 0.0f
    };
    context->encoder.begin_render_pass(RenderPassDescriptor {
        .extent = context->extent,
        .color_attachments = color_attachments,
        .depth_attachment = depth_attachment
    });

    context->encoder.bind_pipeline(context->pipeline);
    context->encoder.bind_index_buffer(context->index_buffer);

    do {
        auto mesh = it.field<GPUMesh>(1);
        auto material_index = it.field<DynamicUniformIndex<Material>>(2);
        auto transform = it.field<DynamicUniformIndex<GlobalTransform>>(3);

        for (const auto i: it) {
            std::array push_constants {
                context->vertex_buffer_index,
                mesh[i].vertex_offset,
                context->view_buffer_index,
                context->material_buffer_index,
                material_index[i].offset,
                context->transform_buffer_index,
                transform[i].offset,
                context->light_buffer_index,
                static_cast<uint32_t>(context->light_buffer.size / sizeof(Light)),
                context->joint_buffer_index,
            };
            context->encoder.set_push_constants(push_constants);
            if (mesh->index_count.has_value()) {
                context->encoder.draw_indexed(mesh[i].index_count.value(), 1, mesh[i].index_offset.value(), 0, 0);
            } else {
                context->encoder.draw(mesh[i].vertex_count, 1, 0, 0);
            }
        }
    } while (it.next());

    context->encoder.end_render_pass();

    {
        std::array barriers {
            TextureBarrier {
                .texture = &surface_texture.texture,
                .range = ImageSubresourceRange {
                    .aspect = FormatAspect::Color,
                    .base_mip_level = 0,
                    .mip_level_count = 1,
                    .base_array_layer = 0,
                    .array_layer_count = 1
                },
                .before = TextureUsage::RenderTarget,
                .after = TextureUsage::Present
            }
        };
        context->encoder.transition_textures(barriers);
    }
    
    auto command_buffer = context->encoder.end_encoding().unwrap();

    std::array wait_semaphores { context->swapchain_semaphore };
    std::array signal_semaphores { context->render_semaphore };
    std::array command_buffers { command_buffer };
    context->queue.submit(command_buffers, wait_semaphores, signal_semaphores, context->render_fence).unwrap();

    // TODO: Check for window closure
    auto _ = context->queue.present(context->surface, surface_texture, signal_semaphores);
    context->device.wait_for_fence(context->render_fence).unwrap();
    context->encoder.reset_all(command_buffers);
}

void prepare_meshes(flecs::iter& it) {
    std::vector<Vertex> all_vertices{};
    std::vector<uint32_t> all_indices{};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto mesh = it.field<Mesh>(1);

        for (const auto i: it) {
            const uint32_t vertex_offset = all_vertices.size();
            const uint32_t index_offset = all_indices.size();
            all_vertices.insert(all_vertices.end(), mesh[i].vertices.begin(), mesh[i].vertices.end());
            GPUMesh gpu_mesh {
                .vertex_count = static_cast<uint32_t>(mesh[i].vertices.size()),
                .vertex_offset = vertex_offset
            };
            if (mesh[i].indices.has_value()) {
                all_indices.insert(all_indices.end(), mesh[i].indices.value().begin(), mesh[i].indices.value().end());
                gpu_mesh.index_count = mesh[i].indices.value().size();
                gpu_mesh.index_offset = index_offset;
            }
            it.entity(i).set(gpu_mesh);
        }
    } while(it.next());

    context->vertex_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_vertices.size() * sizeof(Vertex),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).unwrap();
    {
        void* data = context->device.map_buffer(context->vertex_buffer);
        memcpy(data, all_vertices.data(), all_vertices.size() * sizeof(Vertex));
        context->device.unmap_buffer(context->vertex_buffer);
    }
    context->vertex_buffer_index = context->device.add_binding(context->vertex_buffer);

    context->index_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_indices.size() * sizeof(uint32_t),
        .usage = BufferUsage::Index | BufferUsage::MapReadWrite
    }).unwrap();
    {
        void* data = context->device.map_buffer(context->index_buffer);
        memcpy(data, all_indices.data(), all_indices.size() * sizeof(uint32_t));
        context->device.unmap_buffer(context->index_buffer);
    }
}

void prepare_materials(flecs::iter& it) {
    std::vector<GPUMaterial> all_materials {};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto material = it.field<Material>(1);
        for (const auto i: it) {
            uint32_t index = all_materials.size();
            const GPUTexture* texture = material[i].color_texture.get<GPUTexture>();
            const GPUSampler* sampler = material[i].color_sampler.get<GPUSampler>();
            all_materials.push_back(GPUMaterial {
                .color = material[i].color,
                .color_texture = texture->binding,
                .color_sampler = sampler->binding
            });
            it.entity(i).set<DynamicUniformIndex<Material>>({ index });
        }
    } while (it.next());

    context->material_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_materials.size() * sizeof(Material),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).unwrap();
    {
        void* data = context->device.map_buffer(context->material_buffer);
        memcpy(data, all_materials.data(), all_materials.size() * sizeof(Material));
        context->device.unmap_buffer(context->material_buffer);
    }
    context->material_buffer_index = context->device.add_binding(context->material_buffer);
}

void prepare_transforms(flecs::iter& it) {
    std::vector<glm::mat4> all_transforms{};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto transform = it.field<GlobalTransform>(1);
        for (const auto i: it) {
            uint32_t index = all_transforms.size();
            all_transforms.push_back(glm::inverse(transform[i].transform));
            it.entity(i).set<DynamicUniformIndex<GlobalTransform>>({ index });
        }
    } while (it.next());

    if (context->transform_buffer.buffer == VK_NULL_HANDLE) {
        context->transform_buffer = context->device.create_buffer(BufferDescriptor {
            .size = all_transforms.size() * sizeof(GlobalTransform),
            .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
        }).unwrap();
        context->transform_buffer_index = context->device.add_binding(context->transform_buffer);
    }
    {
        void* data = context->device.map_buffer(context->transform_buffer);
        memcpy(data, all_transforms.data(), all_transforms.size() * sizeof(glm::mat4));
        context->device.unmap_buffer(context->transform_buffer);
    }
}

void prepare_joints(flecs::iter& it) {
    std::vector<glm::mat4> all_joints(100);

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto joint = it.field<Joint>(1);
        auto transform = it.field<GlobalTransform>(2);
        for (const auto i: it) {
            all_joints[joint[i].buffer_offset] = transform[i].transform * joint[i].inverse_bind;
        }
    } while (it.next());

    if (context->joint_buffer.buffer == VK_NULL_HANDLE) {
        context->joint_buffer = context->device.create_buffer(BufferDescriptor {
            .size = all_joints.size() * sizeof(glm::mat4),
            .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
        }).unwrap();
        context->joint_buffer_index = context->device.add_binding(context->joint_buffer);
    }
    {
        void* data = context->device.map_buffer(context->joint_buffer);
        memcpy(data, all_joints.data(), all_joints.size() * sizeof(glm::mat4));
        context->device.unmap_buffer(context->joint_buffer);
    }
}

void prepare_lights(flecs::iter& it) {
    std::vector<Light> all_lights{};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto light = it.field<Light>(1);
        for (const auto i: it) {
            uint32_t index = all_lights.size();
            all_lights.push_back(light[i]);
            it.entity(i).set<DynamicUniformIndex<Light>>({ index });
        }
    } while (it.next());

    context->light_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_lights.size() * sizeof(Light),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).unwrap();
    {
        void* data = context->device.map_buffer(context->light_buffer);
        memcpy(data, all_lights.data(), all_lights.size() * sizeof(Light));
        context->device.unmap_buffer(context->light_buffer);
    }
    context->light_buffer_index = context->device.add_binding(context->light_buffer);
}

void prepare_textures(flecs::iter& it) {
    std::vector<Buffer> texture_buffers;
    std::vector<Texture> textures;
    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto cpu_texture = it.field<CPUTexture>(1);
        for (const auto i: it) {
            Buffer buffer = context->device.create_buffer(BufferDescriptor {
                .size = cpu_texture[i].width * cpu_texture[i].height * 4,
                .usage = BufferUsage::Storage | BufferUsage::MapReadWrite | BufferUsage::TransferSrc
            }).unwrap();
            {
                void* data = context->device.map_buffer(buffer);
                memcpy(data, cpu_texture[i].data.data(), cpu_texture[i].data.size());
                context->device.unmap_buffer(buffer);
            }
            Texture texture = context->device.create_texture(TextureDescriptor {
                .size = Extent3d {
                    .width = cpu_texture[i].width,
                    .height = cpu_texture[i].height,
                    .depth_or_array_layers = 1
                },
                .format = TextureFormat::Rgba8Unorm,
                .usage = TextureUsage::Resource | TextureUsage::CopyDst,
                .dimension = TextureDimension::D2,
                .mip_level_count = 1,
                .sample_count = 1
            }).unwrap();
            TextureView texture_view = context->device.create_texture_view(texture, TextureViewDescriptor {
                .usage = TextureUsage::Resource,
                .dimension = TextureDimension::D2,
                .range = ImageSubresourceRange {
                    .aspect = FormatAspect::Color,
                    .base_mip_level = 0,
                    .mip_level_count = 1,
                    .base_array_layer = 0,
                    .array_layer_count = 1
                }
            }).unwrap();
            const uint32_t binding_index = context->device.add_binding(texture_view);
            it.entity(i).set<GPUTexture>(GPUTexture {
                .texture = texture,
                .view = texture_view,
                .binding = binding_index
            });

            texture_buffers.push_back(buffer);
            textures.push_back(texture);
        }
    } while (it.next());

    context->encoder.begin_encoding().unwrap();
    for (uint32_t i = 0; i < textures.size(); i++) {
        {
            std::array barriers {
                TextureBarrier {
                    .texture = &textures[i],
                    .range = ImageSubresourceRange {
                        .aspect = FormatAspect::Color,
                        .base_mip_level = 0,
                        .mip_level_count = 1,
                        .base_array_layer = 0,
                        .array_layer_count = 1
                    },
                    .before = TextureUsage::Undefined,
                    .after = TextureUsage::CopyDst
                }
            };
            context->encoder.transition_textures(barriers);
        }
        context->encoder.copy_buffer_to_texture(texture_buffers[i], textures[i], TextureUsage::CopyDst);
        {
            std::array barriers {
                TextureBarrier {
                    .texture = &textures[i],
                    .range = ImageSubresourceRange {
                        .aspect = FormatAspect::Color,
                        .base_mip_level = 0,
                        .mip_level_count = 1,
                        .base_array_layer = 0,
                        .array_layer_count = 1
                    },
                    .before = TextureUsage::CopyDst,
                    .after = TextureUsage::ShaderReadOnly
                }
            };
            context->encoder.transition_textures(barriers);
        }
    }
    const auto command_buffer = context->encoder.end_encoding().unwrap();

    std::array command_buffers { command_buffer };
    context->queue.submit(command_buffers, {}, {}, context->render_fence).unwrap();

    context->device.wait_for_fence(context->render_fence).unwrap();
    context->encoder.reset_all(command_buffers);

    for (const auto& buffer: texture_buffers) {
        context->device.destroy_buffer(buffer);
    }
}

void prepare_sampler(flecs::entity entity, RenderContext& context, const CPUSampler& cpu_sampler) {
    const Sampler sampler = context.device.create_sampler(SamplerDescriptor {
        .min_filter = cpu_sampler.min_filter,
        .mag_filter = cpu_sampler.mag_filter
    }).unwrap();
    const uint32_t binding = context.device.add_binding(sampler);
    entity.set<GPUSampler>(GPUSampler {
        .sampler = sampler,
        .binding = binding
    });
}

export Result<void, VkResult> initialize_vulkan(const flecs::world& world) {
    Instance instance{};
    if (const auto res = instance.initialize(InstanceDescriptor{
        .validation = true,
        .gpu_based_validation = true
    }); res.is_err()) {
        return res;
    }

    std::vector<Adapter> adapters = instance.enumerate_adapters().unwrap();
    const Adapter adapter = *std::ranges::find_if(adapters.begin(), adapters.end(), [](const auto& a) { return a.info.type == DeviceType::Gpu; });

    auto open_device = adapter.open().unwrap();
    Device device = std::get<0>(std::move(open_device));
    const Queue queue = std::get<1>(open_device);

    const Window* window = world.get<Window>();
    auto surface_res = instance.create_surface(window->hwnd, window->hinstance);
    if (surface_res.is_err()) {
        return Err(surface_res.unwrap_err());
    }
    Surface surface = surface_res.unwrap();
    SurfaceConfiguration surface_config {
        .extent = Extent3d {
            .width = window->width,
            .height = window->height,
            .depth_or_array_layers = 1
        },
        .present_mode = PresentMode::Mailbox,
        .composite_alpha = CompositeAlphaMode::Opaque,
        .format = TextureFormat::Rgba8Unorm
    };
    if (const auto res = surface.configure(device, queue, surface_config); res.is_err()) {
        return res;
    }

    auto command_res = device.create_command_encoder(CommandEncoderDescriptor { .queue = &queue });
    if (command_res.is_err()) {
        return Err(command_res.unwrap_err());
    }
    CommandEncoder encoder = command_res.unwrap();

    auto fence_res = device.create_fence(false);
    if (fence_res.is_err()) {
        return Err(fence_res.unwrap_err());
    }
    Fence render_fence = fence_res.unwrap();

    auto semaphore_res = device.create_semaphore();
    if (semaphore_res.is_err()) {
        return Err(semaphore_res.unwrap_err());
    }
    Semaphore swapchain_semaphore = semaphore_res.unwrap();

    semaphore_res = device.create_semaphore();
    if (semaphore_res.is_err()) {
        return Err(semaphore_res.unwrap_err());
    }
    Semaphore render_semaphore = semaphore_res.unwrap();

    auto shader_file = read_file("../../assets/shaders/mesh.hlsl");
    ShaderModule vertex_shader = device.create_shader_module(ShaderModuleDescriptor {
        .code = shader_file,
        .entrypoint = "VSMain",
        .stage = ShaderStage::Vertex
    }).unwrap();
    ShaderModule fragment_shader = device.create_shader_module(ShaderModuleDescriptor {
        .code = shader_file,
        .entrypoint = "PSMain",
        .stage = ShaderStage::Fragment
    }).unwrap();

    Pipeline pipeline = device.create_graphics_pipeline(PipelineDescriptor{
        .vertex_shader = &vertex_shader,
        .fragment_shader = &fragment_shader,
        .render_format = TextureFormat::Rgba8Unorm,
        .depth_stencil = DepthStencilState {
            .format = TextureFormat::D32,
            .depth_write_enabled = true,
            .compare = CompareFunction::GreaterEqual
        }
    }).unwrap();

    device.destroy_shader_module(vertex_shader);
    device.destroy_shader_module(fragment_shader);

    world.component<Mesh>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<GPUMesh>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<Material>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<DynamicUniformIndex<Material>>().add(flecs::OnInstantiate, flecs::Inherit);

    float aspect_ratio = static_cast<float>(window->width) / static_cast<float>(window->height);
    ViewUniform view {
        .projection = glm::perspectiveLH(glm::radians(60.0f), aspect_ratio, 10000.0f, 0.01f),
        .view = glm::lookAtLH(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        .position = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f)
    };
    Buffer view_buffer = device.create_buffer(BufferDescriptor {
        .size = sizeof(ViewUniform),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).unwrap();
    uint32_t view_buffer_index = device.add_binding(view_buffer);
    {
        void* data = device.map_buffer(view_buffer);
        memcpy(data, &view, sizeof(ViewUniform));
        device.unmap_buffer(view_buffer);
    }

    Texture depth_texture = device.create_texture(TextureDescriptor {
        .size = Extent3d {
            .width = window->width,
            .height = window->height,
            .depth_or_array_layers = 1
        },
        .format = TextureFormat::D32,
        .usage = TextureUsage::DepthWrite,
        .dimension = TextureDimension::D2,
        .mip_level_count = 1,
        .sample_count = 1
    }).unwrap();
    TextureView depth_texture_view = device.create_texture_view(depth_texture, TextureViewDescriptor {
        .usage = TextureUsage::DepthWrite,
        .dimension = TextureDimension::D2,
        .range = ImageSubresourceRange {
            .aspect = FormatAspect::Depth,
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1
        }
    }).unwrap();

    Extent3d extent {
        .width = window->width,
        .height = window->height,
        .depth_or_array_layers = 1 
    };

    RenderContext context{
        .extent = extent,
        .instance = instance,
        .adapter = adapter,
        .device = std::move(device),
        .queue = queue,
        .surface = std::move(surface),
        .encoder = std::move(encoder),
        .render_fence = render_fence,
        .swapchain_semaphore = swapchain_semaphore,
        .render_semaphore = render_semaphore,
        .pipeline = pipeline,
        .view_buffer = view_buffer,
        .depth_texture = depth_texture,
        .depth_texture_view = depth_texture_view,
        .view_buffer_index = view_buffer_index
    };
    world.set(context);

    world.system<RenderContext, GPUMesh, DynamicUniformIndex<Material>, DynamicUniformIndex<GlobalTransform>>("Render")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::OnStore)
        .run(render);

    world.system<RenderContext, Mesh>("Prepare Meshes")
        .term_at(0).singleton().inout(flecs::InOut)
        .term_at(1).self()
        .kind(flecs::OnStart)
        .run(prepare_meshes);

    world.system<RenderContext, CPUSampler>("Prepare Samplers")
        .term_at(0).singleton().inout(flecs::InOut)
        .write<GPUSampler>()
        .kind(flecs::OnStart)
        .each(prepare_sampler);

    world.system<RenderContext, CPUTexture>("Prepare Textures")
        .term_at(0).singleton().inout(flecs::InOut)
        .write<GPUTexture>()
        .kind(flecs::OnStart)
        .run(prepare_textures);

    world.system<RenderContext, Material>("Prepare Materials")
        .term_at(0).singleton().inout(flecs::InOut)
        .term_at(1).self()
        .read<GPUTexture>()
        .kind(flecs::OnStart)
        .run(prepare_materials);

    world.system<RenderContext, GlobalTransform>("Prepare Transforms")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::PreStore)
        .run(prepare_transforms);

    world.system<RenderContext, Joint, GlobalTransform>("Prepare Joints")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::PreStore)
        .run(prepare_joints);

    world.system<RenderContext, Light>("Prepare Lights")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::OnStart)
        .run(prepare_lights);

    return Ok();
}

export void destroy_vulkan(const flecs::world& world) {
    RenderContext* context = world.get_mut<RenderContext>();

    world.query<GPUTexture>().each([&](const GPUTexture& texture) {
        context->device.destroy_texture_view(texture.view);
        context->device.destroy_texture(texture.texture);
    });
    world.query<GPUSampler>().each([&](const GPUSampler& sampler) {
        context->device.destroy_sampler(sampler.sampler);
    });

    context->device.destroy_texture_view(context->depth_texture_view);
    context->device.destroy_texture(context->depth_texture);
    context->device.destroy_buffer(context->joint_buffer);
    context->device.destroy_buffer(context->light_buffer);
    context->device.destroy_buffer(context->transform_buffer);
    context->device.destroy_buffer(context->material_buffer);
    context->device.destroy_buffer(context->view_buffer);
    context->device.destroy_buffer(context->index_buffer);
    context->device.destroy_buffer(context->vertex_buffer);
    context->device.destroy_pipeline(context->pipeline);
    context->device.destroy_semaphore(context->render_semaphore);
    context->device.destroy_semaphore(context->swapchain_semaphore);
    context->device.destroy_fence(context->render_fence);
    context->device.destroy_command_encoder(context->encoder);
    context->device.destroy_swapchain(context->surface.swapchain);
    context->device.destroy();
    context->instance.destroy_surface(context->surface);
    context->instance.destroy();
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    size_t file_size = file.tellg();
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    std::string res(buffer.data(), buffer.size());
    return res;
}