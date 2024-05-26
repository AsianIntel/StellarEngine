module;

#include "ecs/ecs.hpp"
#include <array>
#include <vulkan/vulkan.hpp>
#include <expected>
#include <fstream>
#include <glm/glm.hpp>

export module stellar.render.vulkan.plugin;

import stellar.render.vulkan;
import stellar.render.types;
import stellar.window;

std::string read_file(const std::string& filename);

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
};

struct Vertex {
    glm::vec4 position;
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
    context.encoder.begin_render_pass(RenderPassDescriptor {
        .extent = context.extent,
        .color_attachments = color_attachments
    });

    context.encoder.bind_pipeline(context.pipeline);
    std::array<uint32_t, 1> push_constants { 0 };
    context.encoder.set_push_constants(push_constants);
    context.encoder.draw(3, 1, 0, 0);

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
        .render_format = TextureFormat::Rgba8Unorm
    }).value();

    device.destroy_shader_module(vertex_shader);
    device.destroy_shader_module(fragment_shader);

    std::array vertices {
        Vertex { .position = glm::vec4(0.0, 0.5, 0.0, 1.0) },
        Vertex { .position = glm::vec4(-0.5, -0.5, 0.0, 1.0) },
        Vertex { .position = glm::vec4(0.5, -0.5, 0.0, 1.0) }
    };
    Buffer vertex_buffer = device.create_buffer(BufferDescriptor {
        .size = vertices.size() * sizeof(Vertex),
        .usage = BufferUsage::Storage | BufferUsage::MapReadWrite
    }).value();
    device.add_binding(vertex_buffer);
    {
        void* data = device.map_buffer(vertex_buffer);
        memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
        device.unmap_buffer(vertex_buffer);
    }

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
        .vertex_buffer = vertex_buffer
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