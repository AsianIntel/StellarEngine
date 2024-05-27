module;

#include "ecs/ecs.hpp"
#include <array>
#include <vulkan/vulkan.hpp>
#include <expected>
#include <fstream>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

export module stellar.render.vulkan.plugin;

import stellar.render.vulkan;
import stellar.render.types;
import stellar.render.primitives;
import stellar.window;

std::string read_file(const std::string& filename);

struct ViewUniform {
    glm::mat4 projection;
    glm::mat4 view;
};

struct GPUMesh {
    uint32_t vertex_count;
    uint32_t vertex_offset;
    std::optional<uint32_t> index_count;
    std::optional<uint32_t> index_offset;
};

export struct Material {
    glm::vec4 color;
};

export struct Transform {
    glm::mat4 transform;
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
    Texture depth_texture{};
    TextureView depth_texture_view{};

    uint32_t vertex_buffer_index{};
    uint32_t view_buffer_index{};
    uint32_t material_buffer_index{};
    uint32_t transform_buffer_index{};
};

void render(flecs::iter& it) {
    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    
    SurfaceTexture surface_texture = context->surface.acquire_texture(context->swapchain_semaphore).value();
    context->encoder.begin_encoding().value();

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
        auto material = it.field<DynamicUniformIndex<Material>>(2);
        auto transform = it.field<DynamicUniformIndex<Transform>>(3);

        for (const auto i: it) {
            std::array push_constants {
                context->vertex_buffer_index,
                mesh[i].vertex_offset,
                context->view_buffer_index,
                context->material_buffer_index,
                material[i].offset,
                context->transform_buffer_index,
                transform[i].offset
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
    
    auto command_buffer = context->encoder.end_encoding().value();

    std::array wait_semaphores { context->swapchain_semaphore };
    std::array signal_semaphores { context->render_semaphore };
    std::array command_buffers { command_buffer };
    context->queue.submit(command_buffers, wait_semaphores, signal_semaphores, context->render_fence).value();

    // TODO: Check for window closure
    auto _ = context->queue.present(context->surface, surface_texture, signal_semaphores);
    context->device.wait_for_fence(context->render_fence).value();
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
    }).value();
    {
        void* data = context->device.map_buffer(context->vertex_buffer);
        memcpy(data, all_vertices.data(), all_vertices.size() * sizeof(Vertex));
        context->device.unmap_buffer(context->vertex_buffer);
    }
    context->vertex_buffer_index = context->device.add_binding(context->vertex_buffer);

    context->index_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_indices.size() * sizeof(uint32_t),
        .usage = BufferUsage::Index | BufferUsage::MapReadWrite
    }).value();
    {
        void* data = context->device.map_buffer(context->index_buffer);
        memcpy(data, all_indices.data(), all_indices.size() * sizeof(uint32_t));
        context->device.unmap_buffer(context->index_buffer);
    }
}

void prepare_materials(flecs::iter& it) {
    std::vector<Material> all_materials {};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto material = it.field<Material>(1);
        for (const auto i: it) {
            uint32_t index = all_materials.size();
            all_materials.push_back(material[i]);
            it.entity(i).set<DynamicUniformIndex<Material>>({ index });
        }
    } while (it.next());

    context->material_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_materials.size() * sizeof(Material),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).value();
    {
        void* data = context->device.map_buffer(context->material_buffer);
        memcpy(data, all_materials.data(), all_materials.size() * sizeof(Material));
        context->device.unmap_buffer(context->material_buffer);
    }
    context->material_buffer_index = context->device.add_binding(context->material_buffer);
}

void prepare_transforms(flecs::iter& it) {
    std::vector<Transform> all_transforms{};

    if (!it.next()) return;
    auto context = it.field<RenderContext>(0);
    do {
        auto transform = it.field<Transform>(1);
        for (const auto i: it) {
            uint32_t index = all_transforms.size();
            all_transforms.push_back(transform[i]);
            it.entity(i).set<DynamicUniformIndex<Transform>>({ index });
        }
    } while (it.next());

    context->transform_buffer = context->device.create_buffer(BufferDescriptor {
        .size = all_transforms.size() * sizeof(Transform),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).value();
    {
        void* data = context->device.map_buffer(context->transform_buffer);
        memcpy(data, all_transforms.data(), all_transforms.size() * sizeof(Transform));
        context->device.unmap_buffer(context->transform_buffer);
    }
    context->transform_buffer_index = context->device.add_binding(context->transform_buffer);
}

export std::expected<void, VkResult> initialize_vulkan(const flecs::world& world) {
    Instance instance{};
    if (const auto res = instance.initialize(InstanceDescriptor{
        .validation = true,
        .gpu_based_validation = true
    }); !res.has_value()) {
        return res;
    }

    std::vector<Adapter> adapters = instance.enumerate_adapters().value();
    const Adapter adapter = *std::ranges::find_if(adapters.begin(), adapters.end(), [](const auto& a) { return a.info.type == DeviceType::Gpu; });

    auto open_device = adapter.open().value();
    Device device = std::get<0>(std::move(open_device));
    const Queue queue = std::get<1>(open_device);

    const Window* window = world.get<Window>();
    auto surface_res = instance.create_surface(window->hwnd, window->hinstance);
    if (!surface_res.has_value()) {
        return std::unexpected(surface_res.error());
    }
    Surface surface = surface_res.value();
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
    if (const auto res = surface.configure(device, queue, surface_config); !res.has_value()) {
        return res;
    }

    auto command_res = device.create_command_encoder(CommandEncoderDescriptor { .queue = &queue });
    if (!command_res.has_value()) {
        return std::unexpected(command_res.error());
    }
    CommandEncoder encoder = command_res.value();

    auto fence_res = device.create_fence(false);
    if (!fence_res.has_value()) {
        return std::unexpected(fence_res.error());
    }
    Fence render_fence = fence_res.value();

    auto semaphore_res = device.create_semaphore();
    if (!semaphore_res.has_value()) {
        return std::unexpected(semaphore_res.error());
    }
    Semaphore swapchain_semaphore = semaphore_res.value();

    semaphore_res = device.create_semaphore();
    if (!semaphore_res.has_value()) {
        return std::unexpected(semaphore_res.error());
    }
    Semaphore render_semaphore = semaphore_res.value();

    auto shader_file = read_file("../assets/shaders/triangle.hlsl");
    ShaderModule vertex_shader = device.create_shader_module(ShaderModuleDescriptor {
        .code = shader_file,
        .entrypoint = "VSMain",
        .stage = ShaderStage::Vertex
    }).value();
    ShaderModule fragment_shader = device.create_shader_module(ShaderModuleDescriptor {
        .code = shader_file,
        .entrypoint = "PSMain",
        .stage = ShaderStage::Fragment
    }).value();

    Pipeline pipeline = device.create_graphics_pipeline(PipelineDescriptor{
        .vertex_shader = &vertex_shader,
        .fragment_shader = &fragment_shader,
        .render_format = TextureFormat::Rgba8Unorm,
        .depth_stencil = DepthStencilState {
            .format = TextureFormat::D32,
            .depth_write_enabled = true,
            .compare = CompareFunction::GreaterEqual
        }
    }).value();

    device.destroy_shader_module(vertex_shader);
    device.destroy_shader_module(fragment_shader);

    world.component<Mesh>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<GPUMesh>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<Material>().add(flecs::OnInstantiate, flecs::Inherit);
    world.component<DynamicUniformIndex<Material>>().add(flecs::OnInstantiate, flecs::Inherit);

    float aspect_ratio = static_cast<float>(window->width) / static_cast<float>(window->height);
    ViewUniform view {
        .projection = glm::perspectiveLH(glm::radians(60.0f), aspect_ratio, 10000.0f, 0.01f),
        .view = glm::lookAtLH(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))
    };
    Buffer view_buffer = device.create_buffer(BufferDescriptor {
        .size = sizeof(ViewUniform),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).value();
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
    }).value();
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
    }).value();

    Extent3d extent {
        .width = window->width,
        .height = window->height,
        .depth_or_array_layers = 1 
    };

    flecs::entity cube_mesh = world.entity("Cube Asset").set<Mesh>(cube(0.5));
    flecs::entity material = world.entity("Material").set<Material>(Material { .color = glm::vec4(0.0, 0.0, 1.0, 1.0) });

    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0, 0.0, 0.0));
        world.entity("Cube 1")
            .is_a(cube_mesh)
            .is_a(material)
            .set<Transform>(Transform { .transform = transform });
    }
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0, 0.0, 0.0));
        world.entity("Cube 2")
            .is_a(cube_mesh)
            .is_a(material)
            .set<Transform>(Transform { .transform = transform });
    }

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

    world.system<RenderContext, GPUMesh, DynamicUniformIndex<Material>, DynamicUniformIndex<Transform>>("Render")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::OnStore)
        .run(render);

    world.system<RenderContext, Mesh>("Prepare Meshes")
        .term_at(0).singleton().inout(flecs::InOut)
        .term_at(1).self()
        .kind(flecs::OnStart)
        .run(prepare_meshes);

    world.system<RenderContext, Material>("Prepare Materials")
        .term_at(0).singleton().inout(flecs::InOut)
        .term_at(1).self()
        .kind(flecs::OnStart)
        .run(prepare_materials);

    world.system<RenderContext, Transform>("Prepare Transforms")
        .term_at(0).singleton().inout(flecs::InOut)
        .kind(flecs::OnStart)
        .run(prepare_transforms);

    return {};
}

export void destroy_vulkan(const flecs::world& world) {
    RenderContext* context = world.get_mut<RenderContext>();

    context->device.destroy_textue_view(context->depth_texture_view);
    context->device.destroy_texture(context->depth_texture);
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