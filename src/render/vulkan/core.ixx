module;

#include "ecs/ecs.hpp"
#include <deque>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <expected>
#include <ranges>
#include <vk_mem_alloc.h>

export module stellar.render.vulkan;

import stellar.render.types;
import stellar.render.vulkan.shader;

export struct Adapter;
export struct Device;
export struct Queue;
export struct Surface;
export struct Swapchain;
export struct CommandEncoder;
export struct CommandBuffer;
export struct TextureView;
export struct Texture;
export struct SurfaceTexture;
export struct Fence;
export struct Semaphore;
export struct Buffer;
export struct ShaderModule;
export struct Pipeline;

constexpr VkFormat map_texture_format(TextureFormat format);
constexpr VkCompositeAlphaFlagBitsKHR map_composite_alpha(CompositeAlphaMode alpha);
constexpr VkPresentModeKHR map_present_mode(PresentMode mode);
constexpr VkImageLayout map_texture_layout(TextureUsage usage);
constexpr VkImageAspectFlagBits map_format_aspect(FormatAspect aspect);
constexpr VkBufferUsageFlags map_buffer_usage(BufferUsage usage);

export struct InstanceDescriptor {
    bool validation;
    bool gpu_based_validation;
};

export struct CommandEncoderDescriptor {
    const Queue* queue;  
};

export struct Attachment {
    TextureView* view;
};

export struct ColorAttachment {
    Attachment target;
    AttachmentOps ops;
    Color clear;
};

export struct RenderPassDescriptor {
    Extent3d extent;
    std::span<ColorAttachment> color_attachments;
};

export struct TextureBarrier {
    Texture* texture;
    ImageSubresourceRange range;
    TextureUsage before;
    TextureUsage after;
};

export struct BufferDescriptor {
    uint64_t size;
    BufferUsage usage;
};

export struct PipelineDescriptor {
    ShaderModule* vertex_shader;
    ShaderModule* fragment_shader;
    TextureFormat render_format;
};

export struct ShaderModuleDescriptor {
    std::string_view code;
    std::string entrypoint;
    ShaderStage stage;
};

export struct Instance {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    std::expected<void, VkResult> initialize(const InstanceDescriptor& descriptor);
    void destroy();
    
    std::expected<std::vector<Adapter>, VkResult> enumerate_adapters() const;
    std::expected<Surface, VkResult> create_surface(HWND hwnd, HINSTANCE hinstance) const;
    void destroy_surface(const Surface& surface) const;
};

struct Adapter {
    VkPhysicalDevice adapter{};
    VkInstance instance{};
    AdapterInfo info{};

    std::expected<std::pair<Device, Queue>, VkResult> open() const;
};

export struct DescriptorHeap {
    VkDescriptorSet set{};
    size_t capacity{};
    size_t len{};
    std::deque<size_t> freelist{};

    std::expected<void, VkResult> initialize(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout, size_t capacity);
    
    size_t allocate();
    void free(size_t index);
};

struct Device {
    VkDevice device;
    VkPhysicalDevice adapter{};
    VkInstance instance{};
    VmaAllocator allocator{};
    ShaderCompiler shader_compiler{};
    
    VkPipelineLayout bindless_pipeline_layout{};
    VkDescriptorPool bindless_descriptor_pool{};
    VkDescriptorSetLayout buffer_set_layout{};
    DescriptorHeap buffer_heap {};

    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;
    
    std::expected<void, VkResult> initialize();
    void destroy();

    std::expected<void, VkResult> wait_for_fence(const Fence& fence) const;
    size_t add_binding(const Buffer& buffer);
    void* map_buffer(const Buffer& buffer) const;
    void unmap_buffer(const Buffer& buffer) const;

    std::expected<Swapchain, VkResult> create_swapchain(VkSurfaceKHR surface, uint32_t queue_family, const SurfaceConfiguration& config) const;
    void destroy_swapchain(const Swapchain& swapchain) const;
    std::expected<CommandEncoder, VkResult> create_command_encoder(const CommandEncoderDescriptor& descriptor) const;
    void destroy_command_encoder(const CommandEncoder& encoder) const;
    std::expected<Fence, VkResult> create_fence(bool signaled) const;
    void destroy_fence(const Fence& fence) const;
    std::expected<Semaphore, VkResult> create_semaphore() const;
    void destroy_semaphore(const Semaphore& semaphore) const;
    std::expected<Buffer, VkResult> create_buffer(const BufferDescriptor& descriptor) const;
    void destroy_buffer(const Buffer& buffer) const;
    std::expected<Pipeline, VkResult> create_graphics_pipeline(const PipelineDescriptor& descriptor) const;
    void destroy_pipeline(const Pipeline& pipeline) const;
    std::expected<ShaderModule, VkResult> create_shader_module(const ShaderModuleDescriptor& descriptor) const;
    void destroy_shader_module(const ShaderModule& module) const;
};

struct Queue {
    VkQueue queue;
    uint32_t family_index;

    std::expected<void, VkResult> submit(std::span<CommandBuffer> command_buffers, std::span<Semaphore> wait_semaphores, std::span<Semaphore> signal_semaphores, const Fence& fence) const;
    std::expected<void, VkResult> present(const Surface& surface, const SurfaceTexture& surface_texture, std::span<Semaphore> wait_semaphores) const;
};

struct Semaphore {
    VkSemaphore semaphore{};
};

struct Fence {
    VkFence fence{};
};

struct Swapchain {
    VkDevice device{};
    VkSwapchainKHR swapchain{};
    std::vector<SurfaceTexture> swapchain_images{};
};

struct Surface {
    VkSurfaceKHR surface{};
    Swapchain swapchain{};

    std::expected<void, VkResult> configure(const Device& device, const Queue& queue, const SurfaceConfiguration& config);
    std::expected<SurfaceTexture, VkResult> acquire_texture(const Semaphore& semaphore);
};

struct CommandEncoder {
    VkCommandPool pool{};
    VkDevice device{};
    std::deque<VkCommandBuffer> free{};
    VkCommandBuffer active{};

    VkDescriptorSet bindless_buffer_set{};
    VkPipelineLayout bindless_pipeline_layout{};

    std::expected<void, VkResult> begin_encoding();
    void begin_render_pass(const RenderPassDescriptor& descriptor) const;
    void transition_textures(const std::span<TextureBarrier>& barriers) const;
    void bind_pipeline(const Pipeline& pipeline) const;
    void set_push_constants(const std::span<uint32_t>& push_constants) const;
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const;
    void end_render_pass() const;
    std::expected<CommandBuffer, VkResult> end_encoding();
    void reset_all(const std::span<CommandBuffer>& command_buffers);
};

struct CommandBuffer {
    VkCommandBuffer buffer;
};

struct Texture {
    VkImage texture{};
};

struct TextureView {
    VkImageView view{};
};

struct SurfaceTexture {
    Texture texture;
    TextureView view;
    uint32_t swapchain_index;
};

struct Buffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};
};

struct ShaderModule {
    VkShaderModule module{};
    std::string entrypoint{};
};

struct Pipeline {
    VkPipeline pipeline{};
    VkPipelineBindPoint bind_point{};
};

std::expected<void, VkResult> Surface::configure(const Device& device, const Queue& queue, const SurfaceConfiguration& config) {
    const auto res = device.create_swapchain(surface, queue.family_index, config);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }
    swapchain = res.value();

    return {};
}

std::expected<SurfaceTexture, VkResult> Surface::acquire_texture(const Semaphore& semaphore) {
    uint32_t image_index;
    if (const auto res = vkAcquireNextImageKHR(swapchain.device, swapchain.swapchain, UINT64_MAX, semaphore.semaphore, VK_NULL_HANDLE, &image_index); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return swapchain.swapchain_images[image_index];
}

std::expected<void, VkResult> Queue::submit(std::span<CommandBuffer> command_buffers, const std::span<Semaphore> wait_semaphores, const std::span<Semaphore> signal_semaphores, const Fence& fence) const {
    std::vector<VkSemaphoreSubmitInfo> wait_infos{};
    std::vector<VkSemaphoreSubmitInfo> signal_infos{};
    std::vector<VkCommandBufferSubmitInfo> buffer_infos{};
    for (const auto semaphore: wait_semaphores) {
        VkSemaphoreSubmitInfo submit_info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        submit_info.semaphore = semaphore.semaphore;
        submit_info.value = 0;
        submit_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        wait_infos.push_back(submit_info);
    }
    for (const auto semaphore: signal_semaphores) {
        VkSemaphoreSubmitInfo submit_info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        submit_info.semaphore = semaphore.semaphore;
        submit_info.value = 0;
        submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signal_infos.push_back(submit_info);
    }
    for (const auto command_buffer: command_buffers) {
        VkCommandBufferSubmitInfo submit_info { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
        submit_info.commandBuffer = command_buffer.buffer;
        buffer_infos.push_back(submit_info);
    }

    VkSubmitInfo2 submit_info { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submit_info.waitSemaphoreInfoCount = wait_infos.size();
    submit_info.pWaitSemaphoreInfos = wait_infos.data();
    submit_info.signalSemaphoreInfoCount = signal_infos.size();
    submit_info.pSignalSemaphoreInfos = signal_infos.data();
    submit_info.commandBufferInfoCount = buffer_infos.size();
    submit_info.pCommandBufferInfos = buffer_infos.data();
    if (const auto res = vkQueueSubmit2(queue, 1, &submit_info, fence.fence); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    return {};
}

std::expected<void, VkResult> Queue::present(const Surface& surface, const SurfaceTexture& surface_texture, std::span<Semaphore> wait_semaphores) const {
    std::vector<VkSemaphore> semaphore_views{};
    std::ranges::transform(wait_semaphores, std::back_inserter(semaphore_views), [](auto semaphore) { return semaphore.semaphore; });
    VkPresentInfoKHR present_info { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = semaphore_views.size();
    present_info.pWaitSemaphores = semaphore_views.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &surface.swapchain.swapchain;
    present_info.pImageIndices = &surface_texture.swapchain_index;

    if (const auto res = vkQueuePresentKHR(queue, &present_info); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    return {};
}

constexpr VkFormat map_texture_format(const TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8Unorm:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::D32:
        return VK_FORMAT_D32_SFLOAT;
    default:
        std::unreachable();        
    }
}

constexpr VkCompositeAlphaFlagBitsKHR map_composite_alpha(const CompositeAlphaMode alpha) {
    switch (alpha) {
    case CompositeAlphaMode::Opaque:
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    case CompositeAlphaMode::PreMuliplied:
        return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    case CompositeAlphaMode::PostMultiplied:
        return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    case CompositeAlphaMode::Inherit:
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    default:
        std::unreachable();
    }
}

constexpr VkPresentModeKHR map_present_mode(const PresentMode mode) {
    switch(mode) {
    case PresentMode::Fifo:
        return VK_PRESENT_MODE_FIFO_KHR;
    case PresentMode::FifoRelaxed:
        return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case PresentMode::Immediate:
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case PresentMode::Mailbox:
        return VK_PRESENT_MODE_MAILBOX_KHR;
    default:
        std::unreachable();
    }
}

constexpr VkImageLayout map_texture_layout(const TextureUsage usage) {
    switch (usage) {
    case TextureUsage::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case TextureUsage::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case TextureUsage::CopySrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case TextureUsage::CopyDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case TextureUsage::RenderTarget:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    default:
        std::unreachable();        
    }
}

constexpr VkImageAspectFlagBits map_format_aspect(const FormatAspect aspect) {
    switch(aspect) {
    case FormatAspect::Color:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case FormatAspect::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        std::unreachable();
    }
}

constexpr VkBufferUsageFlags map_buffer_usage(const BufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if ((usage & BufferUsage::Storage) == BufferUsage::Storage) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if ((usage & BufferUsage::TransferSrc) == BufferUsage::TransferSrc) {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if ((usage & BufferUsage::TransferDst) == BufferUsage::TransferDst) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    if ((usage & BufferUsage::Index) == BufferUsage::Index) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    return flags;
}
