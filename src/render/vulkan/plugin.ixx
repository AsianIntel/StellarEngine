module;

#include "ecs/ecs.hpp"
#include <array>
#include <vulkan/vulkan.hpp>
#include <expected>

export module stellar.render.vulkan.plugin;

import stellar.render.vulkan;
import stellar.render.types;
import stellar.window;

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
};

void render(RenderContext& context) {
    context.device.wait_for_fence(context.render_fence).value();
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

    auto fence_res = device.create_fence(true);
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
        .render_semaphore = render_semaphore
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

    context->device.wait_for_fence(context->render_fence).value();

    context->device.destroy_semaphore(context->render_semaphore);
    context->device.destroy_semaphore(context->swapchain_semaphore);
    context->device.destroy_fence(context->render_fence);
    context->device.destroy_command_encoder(context->encoder);
    context->device.destroy_swapchain(context->surface.swapchain);
    context->device.destroy();
    context->instance.destroy_surface(context->surface);
    context->instance.destroy();
}