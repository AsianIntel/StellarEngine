module;

#include <cstdint>
#include <type_traits>

#define DEFINE_ENUM_OP(type) \
    export constexpr type operator&(type lhs, type rhs) { \
        return static_cast<type>(static_cast<std::underlying_type_t<type>>(lhs) & static_cast<std::underlying_type_t<type>>(rhs)); \
    } \
    export constexpr type operator|(type lhs, type rhs) { \
        return static_cast<type>(static_cast<std::underlying_type_t<type>>(lhs) | static_cast<std::underlying_type_t<type>>(rhs)); \
    } \
    

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
DEFINE_ENUM_OP(AttachmentOps)

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

export enum class TextureDimension {
    D1,
    D2,
    D3
};

export enum class TextureUsage: uint32_t {
    Undefined = 0,
    Present = 1,
    CopySrc = 1 << 1,
    CopyDst = 1 << 2,
    RenderTarget = 1 << 3,
    DepthRead = 1 << 4,
    DepthWrite = 1 << 5,
    ShaderReadOnly = 1 << 6,
    Resource = 1 << 7,
};
DEFINE_ENUM_OP(TextureUsage)

export enum class BufferUsage: uint32_t {
    Storage = 1,
    MapReadWrite = 1 << 1,
    TransferSrc = 1 << 2,
    TransferDst = 1 << 3,
    Index = 1 << 4,
};
DEFINE_ENUM_OP(BufferUsage)

export enum class ShaderStage {
    Vertex = 0,
    Fragment = 1,
    Compute = 2,
};

export struct SurfaceConfiguration {
    Extent3d extent;
    PresentMode present_mode;
    CompositeAlphaMode composite_alpha;
    TextureFormat format;
};

export enum class CompareFunction {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    GreaterEqual,
    NotEqual,
    Always
};

export struct DepthStencilState {
    TextureFormat format;
    bool depth_write_enabled;
    CompareFunction compare;
};

export enum class Filter {
    Nearest,
    Linear
};