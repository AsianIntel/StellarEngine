module;

#include <cstdint>
#include <type_traits>

export module stellar.render.types;

export enum class DeviceType {
    Gpu,
    IntegratedGpu,
    VirtualGpu,
    Cpu,
    Other
};

export struct AdapterInfo {
    DeviceType type{};
};

export struct Extent3d {
    uint32_t width;
    uint32_t height;
    uint32_t depth_or_array_layers;
};

export enum class PresentMode {
    Fifo,
    FifoRelaxed,
    Mailbox,
    Immediate
};

export enum class TextureFormat {
    Rgba8Unorm,
    D32
};

export enum class CompositeAlphaMode {
    Opaque,
    PreMuliplied,
    PostMultiplied,
    Inherit
};

export enum class AttachmentOps: uint32_t {
    Load = 0x1,
    Store = 0x2,
};

export AttachmentOps operator&(AttachmentOps lhs, AttachmentOps rhs) {
    return static_cast<AttachmentOps>(static_cast<std::underlying_type_t<AttachmentOps>>(lhs) & static_cast<std::underlying_type_t<AttachmentOps>>(rhs));
}

export struct Color {
    float r;
    float g;
    float b;
    float a;
};

export enum class FormatAspect {
    Color,
    Depth
};

export struct ImageSubresourceRange {
    FormatAspect aspect;
    uint32_t base_mip_level;
    uint32_t mip_level_count;
    uint32_t base_array_layer;
    uint32_t array_layer_count;
};

export enum class TextureUsage: uint32_t {
    Undefined = 0,
    Present = 0x1,
    CopySrc = 0x2,
    CopyDst = 0x4,
    RenderTarget = 0x8
};

export TextureUsage operator&(TextureUsage lhs, TextureUsage rhs) {
    return static_cast<TextureUsage>(static_cast<std::underlying_type_t<TextureUsage>>(lhs) & static_cast<std::underlying_type_t<TextureUsage>>(rhs));
}

export struct SurfaceConfiguration {
    Extent3d extent;
    PresentMode present_mode;
    CompositeAlphaMode composite_alpha;
    TextureFormat format;
};