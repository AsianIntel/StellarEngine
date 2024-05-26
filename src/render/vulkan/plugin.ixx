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
    Texture depth_texture{};
    TextureView depth_texture_view{};

    uint32_t vertex_buffer_index{};
    uint32_t view_buffer_index{};
};

void render(RenderContext& context) {
    SurfaceTexture surface_texture = context.surface.acquire_texture(context.swapchain_semaphore).value();

    context.encoder.begin_encoding().value();

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
        context.encoder.transition_textures(barriers);
    }

    std::array color_attachments {
        ColorAttachment {
            .target = Attachment { .view = &surface_texture.view },
            .ops = AttachmentOps::Store,
            .clear = Color { 0.0, 0.0, 0.0, 1.0 }
        }
    };
    DepthAttachment depth_attachment {
        .target = Attachment { .view = &context.depth_texture_view },
        .ops = AttachmentOps::Store,
        .depth_clear = 0.0f
    };
    context.encoder.begin_render_pass(RenderPassDescriptor {
        .extent = context.extent,
        .color_attachments = color_attachments,
        .depth_attachment = depth_attachment
    });

    context.encoder.bind_pipeline(context.pipeline);
    context.encoder.bind_index_buffer(context.index_buffer);
    std::array push_constants { context.vertex_buffer_index, context.view_buffer_index };
    context.encoder.set_push_constants(push_constants);
    context.encoder.draw_indexed(36, 1, 0, 0, 0);

    context.encoder.end_render_pass();

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
        context.encoder.transition_textures(barriers);
    }
    
    auto command_buffer = context.encoder.end_encoding().value();

    std::array wait_semaphores { context.swapchain_semaphore };
    std::array signal_semaphores { context.render_semaphore };
    std::array command_buffers { command_buffer };
    context.queue.submit(command_buffers, wait_semaphores, signal_semaphores, context.render_fence).value();

    // TODO: Check for window closure
    auto _ = context.queue.present(context.surface, surface_texture, signal_semaphores);
    context.device.wait_for_fence(context.render_fence).value();
    context.encoder.reset_all(command_buffers);
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

    const Mesh cube_mesh = cube(1.0f);
    Buffer vertex_buffer = device.create_buffer(BufferDescriptor {
        .size = cube_mesh.vertices.size() * sizeof(Vertex),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).value();
    uint32_t vertex_buffer_index = device.add_binding(vertex_buffer);
    {
        void* data = device.map_buffer(vertex_buffer);
        memcpy(data, cube_mesh.vertices.data(), cube_mesh.vertices.size() * sizeof(Vertex));
        device.unmap_buffer(vertex_buffer);
    }
    Buffer index_buffer = device.create_buffer(BufferDescriptor {
        .size = cube_mesh.indices.value().size() * sizeof(uint32_t),
        .usage = BufferUsage::Index | BufferUsage::MapReadWrite
    }).value();
    {
        void* data = device.map_buffer(index_buffer);
        memcpy(data, cube_mesh.indices.value().data(), cube_mesh.indices.value().size() * sizeof(uint32_t));
        device.unmap_buffer(index_buffer);
    }

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
        .vertex_buffer = vertex_buffer,
        .index_buffer = index_buffer,
        .view_buffer = view_buffer,
        .depth_texture = depth_texture,
        .depth_texture_view = depth_texture_view,
        .vertex_buffer_index = vertex_buffer_index,
        .view_buffer_index = view_buffer_index
    };
    world.set(context);

    world.system<RenderContext>("Render")
        .term_at(0).singleton()
        .kind(flecs::OnUpdate)
        .each(render);

    return {};
}

export void destroy_vulkan(const flecs::world& world) {
    RenderContext* context = world.get_mut<RenderContext>();

    context->device.destroy_textue_view(context->depth_texture_view);
    context->device.destroy_texture(context->depth_texture);
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