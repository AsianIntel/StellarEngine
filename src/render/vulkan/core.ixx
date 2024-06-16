module;

#include "ecs/ecs.hpp"
#include <deque>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <optional>
#include <ranges>
#include <vk_mem_alloc.h>

export module stellar.render.vulkan;

import stellar.render.types;
import stellar.render.vulkan.shader;
import stellar.core.result;
import stellar.core;

export struct Adapter;
export struct Device;
export struct Queue;
export struct Surface;
export struct Swapchain;
export struct CommandEncoder;
export struct CommandBuffer;
export struct TextureView;
export struct Texture;
export struct Sampler;
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
constexpr VkImageType map_image_type(TextureDimension dimension);
constexpr VkImageViewType map_image_view_type(TextureDimension dimension);
constexpr VkImageUsageFlags map_texture_usage(TextureUsage usage);
constexpr VkCompareOp map_compare_function(CompareFunction function);
constexpr VkFilter map_filter(Filter filter);

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

export struct DepthAttachment {
    Attachment target;
    AttachmentOps ops;
    float depth_clear;
};

export struct RenderPassDescriptor {
    Extent3d extent;
    std::span<ColorAttachment> color_attachments;
    std::optional<DepthAttachment> depth_attachment;
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

export struct RenderPipelineDescriptor {
    ShaderModule* vertex_shader;
    std::optional<ShaderModule*> fragment_shader;
    std::span<TextureFormat> render_format;
    std::optional<DepthStencilState> depth_stencil;
};

export struct ComputePipelineDescriptor {
    ShaderModule* compute_shader;
};

export struct ShaderModuleDescriptor {
    std::string_view code;
    std::string entrypoint;
    ShaderStage stage;
    std::vector<std::string> defines;
};

export struct TextureDescriptor {
    Extent3d size;
    TextureFormat format;
    TextureUsage usage;
    TextureDimension dimension;
    uint32_t mip_level_count;
    uint32_t sample_count;
};

export struct TextureViewDescriptor {
    TextureUsage usage;
    TextureDimension dimension;
    ImageSubresourceRange range;
};

export struct SamplerDescriptor {
    Filter min_filter;
    Filter mag_filter;
};

export struct Instance {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    Result<void, VkResult> initialize(const InstanceDescriptor& descriptor);
    void destroy();
    
    Result<std::vector<Adapter>, VkResult> enumerate_adapters() const;
    Result<Surface, VkResult> create_surface(HWND hwnd, HINSTANCE hinstance) const;
    void destroy_surface(const Surface& surface) const;
};

struct Adapter {
    VkPhysicalDevice adapter{};
    VkInstance instance{};
    AdapterInfo info{};

    Result<std::pair<Device, Queue>, VkResult> open() const;
};

export struct DescriptorHeap {
    VkDescriptorSet set{};
    size_t capacity{};
    size_t len{};
    std::deque<size_t> freelist{};

    Result<void, VkResult> initialize(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout, size_t capacity);
    
    size_t allocate();
    void free(size_t index);
};

struct Sampler {
    VkSampler sampler{};
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
    VkDescriptorSetLayout texture_set_layout{};
    VkDescriptorSetLayout sampler_set_layout{};
    DescriptorHeap buffer_heap{};
    DescriptorHeap texture_heap{};
    DescriptorHeap sampler_heap{};

    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;
    
    Result<void, VkResult> initialize();
    void destroy();

    Result<void, VkResult> wait_for_fence(const Fence& fence) const;
    size_t add_binding(const Buffer& buffer);
    size_t add_binding(const TextureView& view);
    size_t add_binding(const Sampler& sampler);
    void* map_buffer(const Buffer& buffer) const;
    void unmap_buffer(const Buffer& buffer) const;

    Result<Swapchain, VkResult> create_swapchain(VkSurfaceKHR surface, uint32_t queue_family, const SurfaceConfiguration& config) const;
    void destroy_swapchain(const Swapchain& swapchain) const;
    Result<CommandEncoder, VkResult> create_command_encoder(const CommandEncoderDescriptor& descriptor) const;
    void destroy_command_encoder(const CommandEncoder& encoder) const;
    Result<Fence, VkResult> create_fence(bool signaled) const;
    void destroy_fence(const Fence& fence) const;
    Result<Semaphore, VkResult> create_semaphore() const;
    void destroy_semaphore(const Semaphore& semaphore) const;
    Result<Buffer, VkResult> create_buffer(const BufferDescriptor& descriptor) const;
    void destroy_buffer(const Buffer& buffer) const;
    Result<Pipeline, VkResult> create_graphics_pipeline(const RenderPipelineDescriptor& descriptor) const;
    Result<Pipeline, VkResult> create_compute_pipeline(const ComputePipelineDescriptor& descriptor) const;
    void destroy_pipeline(const Pipeline& pipeline) const;
    Result<ShaderModule, VkResult> create_shader_module(const ShaderModuleDescriptor& descriptor) const;
    void destroy_shader_module(const ShaderModule& module) const;
    Result<Texture, VkResult> create_texture(const TextureDescriptor& descriptor) const;
    void destroy_texture(const Texture& texture) const;
    Result<TextureView, VkResult> create_texture_view(const Texture& texture, const TextureViewDescriptor& descriptor) const;
    void destroy_texture_view(const TextureView& view) const;
    Result<Sampler, VkResult> create_sampler(const SamplerDescriptor& descriptor) const;
    void destroy_sampler(const Sampler& sampler) const;
};

struct Queue {
    VkQueue queue;
    uint32_t family_index;

    Result<void, VkResult> submit(std::span<CommandBuffer> command_buffers, std::span<Semaphore> wait_semaphores, std::span<Semaphore> signal_semaphores, const Fence& fence) const;
    Result<void, VkResult> present(const Surface& surface, const SurfaceTexture& surface_texture, std::span<Semaphore> wait_semaphores) const;
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

    Result<void, VkResult> configure(const Device& device, const Queue& queue, const SurfaceConfiguration& config);
    Result<SurfaceTexture, VkResult> acquire_texture(const Semaphore& semaphore) const;
};

struct CommandEncoder {
    VkCommandPool pool{};
    VkDevice device{};
    std::deque<VkCommandBuffer> free{};
    VkCommandBuffer active{};

    VkDescriptorSet bindless_buffer_set{};
    VkDescriptorSet bindless_texture_set{};
    VkDescriptorSet bindless_sampler_set{};
    VkPipelineLayout bindless_pipeline_layout{};

    Result<void, VkResult> begin_encoding();
    void begin_render_pass(const RenderPassDescriptor& descriptor) const;
    void transition_textures(const std::span<TextureBarrier>& barriers) const;
    void copy_buffer_to_texture(const Buffer& buffer, const Texture& texture, TextureUsage layout) const;
    void bind_pipeline(const Pipeline& pipeline) const;
    void bind_index_buffer(const Buffer& buffer) const;
    void set_push_constants(const std::span<uint32_t>& push_constants) const;
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const;
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) const;
    void dispatch(uint32_t x, uint32_t y, uint32_t z) const;
    void end_render_pass() const;
    Result<CommandBuffer, VkResult> end_encoding();
    void reset_all(const std::span<CommandBuffer>& command_buffers);
};

struct CommandBuffer {
    VkCommandBuffer buffer;
};

struct Texture {
    VkImage texture{};
    VmaAllocation allocation{};
    TextureFormat format{};
    Extent3d size{};
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
    uint64_t size{};
};

struct ShaderModule {
    VkShaderModule module{};
    std::string entrypoint{};
};

struct Pipeline {
    VkPipeline pipeline{};
    VkPipelineBindPoint bind_point{};
};

constexpr VkFormat map_texture_format(const TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8Unorm:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::D32:
        return VK_FORMAT_D32_SFLOAT;
    default:
        unreachable();
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
        unreachable();
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
        unreachable();
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
    case TextureUsage::DepthRead:
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    case TextureUsage::DepthWrite:
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case TextureUsage::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        unreachable();
    }
}

constexpr VkImageAspectFlagBits map_format_aspect(const FormatAspect aspect) {
    switch(aspect) {
    case FormatAspect::Color:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case FormatAspect::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        unreachable();
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

constexpr VkImageType map_image_type(const TextureDimension dimension) {
    switch (dimension) {
    case TextureDimension::D1:
        return VK_IMAGE_TYPE_1D;
    case TextureDimension::D2:
        return VK_IMAGE_TYPE_2D;
    case TextureDimension::D3:
        return VK_IMAGE_TYPE_3D;
    default:
        unreachable();
    }
}

constexpr VkImageViewType map_image_view_type(const TextureDimension dimension) {
    switch (dimension) {
    case TextureDimension::D1:
        return VK_IMAGE_VIEW_TYPE_1D;
    case TextureDimension::D2:
        return VK_IMAGE_VIEW_TYPE_2D;
    case TextureDimension::D3:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        unreachable();
    }
}

constexpr VkImageUsageFlags map_texture_usage(const TextureUsage usage) {
    VkImageUsageFlags flags = 0;
    if ((usage & TextureUsage::RenderTarget) == TextureUsage::RenderTarget) {
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if ((usage & TextureUsage::CopySrc) == TextureUsage::CopySrc) {
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if ((usage & TextureUsage::CopyDst) == TextureUsage::CopyDst) {
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if ((usage & TextureUsage::DepthRead) == TextureUsage::DepthRead) {
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if ((usage & TextureUsage::DepthWrite) == TextureUsage::DepthWrite) {
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if ((usage & TextureUsage::Resource) == TextureUsage::Resource) {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    return flags;
}

constexpr VkCompareOp map_compare_function(const CompareFunction function) {
    switch (function) {
    case CompareFunction::Never:
        return VK_COMPARE_OP_NEVER;
    case CompareFunction::Less:
        return VK_COMPARE_OP_LESS;
    case CompareFunction::Equal:
        return VK_COMPARE_OP_EQUAL;
    case CompareFunction::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareFunction::Greater:
        return VK_COMPARE_OP_GREATER;
    case CompareFunction::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareFunction::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case CompareFunction::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        unreachable();
    }
}

constexpr VkFilter map_filter(const Filter filter) {
    switch(filter) {
    case Filter::Nearest:
        return VK_FILTER_NEAREST;
    case Filter::Linear:
        return VK_FILTER_LINEAR;
    default:
        unreachable();
    }
}
